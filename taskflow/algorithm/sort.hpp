#pragma once

#include "../core/async.hpp"

namespace tf {

// 是否进行并行排序的阈值  
template <typename I>
constexpr size_t parallel_sort_cutoff() {
 
  using value_type = typename std::iterator_traits<I>::value_type;

  constexpr size_t object_size = sizeof(value_type);

  if constexpr(std::is_same_v<value_type, std::string>) {
    return 65536 / sizeof(std::string);
  }
  else {
    if constexpr(object_size < 16) return 4096;
    else if constexpr(object_size < 32) return 2048;
    else if constexpr(object_size < 64) return 1024;
    else if constexpr(object_size < 128) return 768;
    else if constexpr(object_size < 256) return 512;
    else if constexpr(object_size < 512) return 256;
    else return 128;
  }
}

// ----------------------------------------------------------------------------
// pattern-defeating quick sort (pdqsort)
// https://github.com/orlp/pdqsort/
// ----------------------------------------------------------------------------

template<typename T, size_t cacheline_size = 64>
inline T* align_cacheline(T* p) {
#if defined(UINTPTR_MAX) && __cplusplus >= 201103L
  std::uintptr_t ip = reinterpret_cast<std::uintptr_t>(p);
#else
  std::size_t ip = reinterpret_cast<std::size_t>(p);
#endif
  ip = (ip + cacheline_size - 1) & -cacheline_size;
  return reinterpret_cast<T*>(ip);
}


template<typename Iter>
inline void swap_offsets(  Iter first, Iter last, unsigned char* offsets_l, unsigned char* offsets_r,  size_t num, bool use_swaps ) {
  
  typedef typename std::iterator_traits<Iter>::value_type T;
  
  if (use_swaps) {
    // 降序分布需要这种情况，我们需要适当交换 pdqsort 以保持 O(n)。
    for (size_t i = 0; i < num; ++i) {
        std::iter_swap(first + offsets_l[i], last - offsets_r[i]);
    }
  } else if (num > 0) {
    Iter l = first + offsets_l[0]; Iter r = last - offsets_r[0];
    T tmp(std::move(*l)); *l = std::move(*r);
    for (size_t i = 1; i < num; ++i) {
        l = first + offsets_l[i]; *r = std::move(*l);
        r = last - offsets_r[i]; *l = std::move(*r);
    }
    *r = std::move(tmp);
  }
}

// 使用带有给定比较函数的插入排序对 [begin, end) 进行排序
template<typename RandItr, typename Compare>
void insertion_sort(RandItr begin, RandItr end, Compare comp) {

  using T = typename std::iterator_traits<RandItr>::value_type;

  if (begin == end) {
    return;
  }

  for (RandItr cur = begin + 1; cur != end; ++cur) {

    RandItr shift = cur;
    RandItr shift_1 = cur - 1;

    // 首先比较以避免对已经正确定位的元素进行 2 次移动。
    if (comp(*shift, *shift_1)) {
      T tmp = std::move(*shift);
      do {
        *shift-- = std::move(*shift_1);
      }while (shift != begin && comp(tmp, *--shift_1));
      *shift = std::move(tmp);
    }
  }
}



// 使用带有给定比较函数的插入排序对 [begin, end) 进行排序。 假设 *(begin - 1) 是小于或等于 [begin, end) 中的任何元素的元素。
template<typename RandItr, typename Compare>
void unguarded_insertion_sort(RandItr begin, RandItr end, Compare comp) {

  using T = typename std::iterator_traits<RandItr>::value_type;

  if (begin == end) {
    return;
  }

  for (RandItr cur = begin + 1; cur != end; ++cur) {
    RandItr shift = cur;
    RandItr shift_1 = cur - 1;

    // 首先进行比较，这样我们就可以避免对已经正确定位的元素进行 2 次移动。
    if (comp(*shift, *shift_1)) {
      T tmp = std::move(*shift);

      do {
        *shift-- = std::move(*shift_1);
      }while (comp(tmp, *--shift_1));

      *shift = std::move(tmp);
    }
  }
}



// 尝试在 [begin, end) 上使用插入排序。 如果移动的元素超过 partial_insertion_sort_limit 元素，将返回 false，并中止排序。 否则它将成功排序并返回 true。
template<typename RandItr, typename ·>
bool partial_insertion_sort(RandItr begin, RandItr end, Compare comp) {

  using T = typename std::iterator_traits<RandItr>::value_type;
  using D = typename std::iterator_traits<RandItr>::difference_type;

  // 当我们检测到一个已经排序的分区时，尝试插入排序，允许在放弃之前移动这个数量的元素。
  constexpr auto partial_insertion_sort_limit = D{8};

  if (begin == end) return true;

  auto limit = D{0};

  for (RandItr cur = begin + 1; cur != end; ++cur) {

    if (limit > partial_insertion_sort_limit) {
      return false;
    }

    RandItr shift = cur;
    RandItr shift_1 = cur - 1;

    // 首先进行比较，这样我们就可以避免对已经正确定位的元素进行 2 次移动。
    if (comp(*shift, *shift_1)) {
      T tmp = std::move(*shift);

      do {
        *shift-- = std::move(*shift_1);
      }while (shift != begin && comp(tmp, *--shift_1));

      *shift = std::move(tmp);
      limit += cur - shift;
    }
  }

  return true;
}



// 分区 [begin, end) 围绕 pivot *begin 使用比较函数 comp。 等于主元的元素放在右侧分区中。 
// 返回分区后枢轴的位置以及传递的序列是否已正确分区。 假设主元是至少 3 个元素的中位数，并且 [begin, end) 至少是 insertion_sort_threshold 长。 
// Uses branchless partitioning.
template<typename Iter, typename Compare>
std::pair<Iter, bool> partition_right_branchless(Iter begin, Iter end, Compare comp) {

  typedef typename std::iterator_traits<Iter>::value_type T;

  constexpr size_t block_size = 64;
  constexpr size_t cacheline_size = 64;

  // Move pivot into local for speed.
  T pivot(std::move(*begin));
  Iter first = begin;
  Iter last = end;

  // 找到大于或等于主元的第一个元素（3 的中位数保证存在） 
  while (comp(*++first, pivot));

  // 找到严格小于主元的第一个元素。 如果 *first 之前没有元素，我们必须保护这个搜索。
  if (first - 1 == begin) while (first < last && !comp(*--last, pivot));
  else                    while (                !comp(*--last, pivot));

  // 如果应该交换到分区的第一对元素是相同的元素，则传入的序列已经被正确分区。
  bool already_partitioned = first >= last;
  if (!already_partitioned) {
    std::iter_swap(first, last);
    ++first;

    // The following branchless partitioning is derived from "BlockQuicksort: How Branch Mispredictions don't affect Quicksort" by Stefan Edelkamp and Armin Weiss, but heavily micro-optimized.
    unsigned char offsets_l_storage[block_size + cacheline_size];
    unsigned char offsets_r_storage[block_size + cacheline_size];
    unsigned char* offsets_l = align_cacheline(offsets_l_storage);
    unsigned char* offsets_r = align_cacheline(offsets_r_storage);

    Iter offsets_l_base = first;
    Iter offsets_r_base = last;
    size_t num_l, num_r, start_l, start_r;
    num_l = num_r = start_l = start_r = 0;

    while (first < last) {
      // 用错误一侧的元素填充 offset blocks 。 首先，我们确定每个  offset block 考虑了多少元素。
      size_t num_unknown = last - first;
      size_t left_split = num_l == 0 ? (num_r == 0 ? num_unknown / 2 : num_unknown) : 0;
      size_t right_split = num_r == 0 ? (num_unknown - left_split) : 0;

      // Fill the offset blocks.
      if (left_split >= block_size) {
        for (size_t i = 0; i < block_size;) {
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
        }
      } else {
        for (size_t i = 0; i < left_split;) {
          offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
        }
      }

      if (right_split >= block_size) {
        for (size_t i = 0; i < block_size;) {
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
        }
      } else {
        for (size_t i = 0; i < right_split;) {
          offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
        }
      }

      // 交换元素并更新块大小和第一个/最后一个边界。
      size_t num = std::min(num_l, num_r);
      swap_offsets(
        offsets_l_base, offsets_r_base, 
        offsets_l + start_l, offsets_r + start_r,
        num, num_l == num_r
      );
      num_l -= num; num_r -= num;
      start_l += num; start_r += num;

      if (num_l == 0) {
        start_l = 0;
        offsets_l_base = first;
      }

      if (num_r == 0) {
        start_r = 0;
        offsets_r_base = last;
      }
    }

    // 我们现在已经完全确定了 [first, last) 的正确位置。 交换最后一个元素。
    if (num_l) {
      offsets_l += start_l;
      while (num_l--) std::iter_swap(offsets_l_base + offsets_l[num_l], --last);
      first = last;
    }
    if (num_r) {
      offsets_r += start_r;
      while (num_r--) std::iter_swap(offsets_r_base - offsets_r[num_r], first), ++first;
      last = first;
    }
  }

  // 将 pivot 放在正确的位置。  
  Iter pivot_pos = first - 1;
  *begin = std::move(*pivot_pos);
  *pivot_pos = std::move(pivot);

  return std::make_pair(pivot_pos, already_partitioned);
}



// Partitions [begin, end) 围绕 pivot *begin 使用比较函数 comp。 等于主元的元素放在右侧分区中。
// 返回分区后 pivot 的位置以及传递的序列是否已正确分区。
//  假设主元是至少 3 个元素的中位数，并且 [begin, end) 至少是 insertion_sort_threshold 长。  
template<typename Iter, typename Compare>
std::pair<Iter, bool> partition_right(Iter begin, Iter end, Compare comp) {

  using T = typename std::iterator_traits<Iter>::value_type;

  // Move pivot into local for speed.
  T pivot(std::move(*begin));

  Iter first = begin;
  Iter last = end;

  // 找到大于或等于主元的第一个元素（3 个保证的中位数/这个存在）。
  while (comp(*++first, pivot));

  // 找到严格小于主元的第一个元素。 如果 *first 之前没有元素，我们必须保护这个搜索。
  if (first - 1 == begin) while (first < last && !comp(*--last, pivot));
  else while (!comp(*--last, pivot));

  //  如果应该交换到 partition 的第一对元素是相同的元素，则传入的序列已经被正确 partition 。
  bool already_partitioned = first >= last;

  // 继续交换位于 pivot 错误一侧的元素对。 先前交换的对保护搜索，这就是为什么上面的第一次迭代是特例的。  
  while (first < last) {
    std::iter_swap(first, last);
    while (comp(*++first, pivot));
    while (!comp(*--last, pivot));
  }

 // 将 pivot 放在正确的位置。 
  Iter pivot_pos = first - 1;
  *begin = std::move(*pivot_pos);
  *pivot_pos = std::move(pivot);

  return std::make_pair(pivot_pos, already_partitioned);
}

// 与上面的功能类似，除了等于 pivot 的元素被放在 pivot 的左侧，并且它不会检查或返回传递的序列是否已被分区。 
// 由于这很少使用（许多相等的情况），并且在那种情况下 pdqsort 已经具有 O(n) 性能，为了简单起见，这里没有应用块快速排序。
template<typename RandItr, typename Compare>
RandItr partition_left(RandItr begin, RandItr end, Compare comp) {

  using T = typename std::iterator_traits<RandItr>::value_type;

  T pivot(std::move(*begin));

  RandItr first = begin;
  RandItr last = end;

  while (comp(pivot, *--last));

  if (last + 1 == end) {
    while (first < last && !comp(pivot, *++first));
  }
  else {
    while (!comp(pivot, *++first));
  }

  while (first < last) {
    std::iter_swap(first, last);
    while (comp(pivot, *--last));
    while (!comp(pivot, *++first));
  }

  RandItr pivot_pos = last;
  *begin = std::move(*pivot_pos);
  *pivot_pos = std::move(pivot);

  return pivot_pos;
}

template<typename Iter, typename Compare, bool Branchless>
void parallel_pdqsort(
  tf::Runtime& rt,
  Iter begin, Iter end, Compare comp,
  int bad_allowed, bool leftmost = true
) {

  // 小于此大小的 Partitions 按顺序排序  
  constexpr auto cutoff = parallel_sort_cutoff<Iter>();

  // 小于这个大小的 Partitions 使用插入排序 
  constexpr auto insertion_sort_threshold = 24;

  // 大于此大小的 Partitions 使用 Tukey 的第九位来选择 pivot 
  constexpr auto ninther_threshold = 128;
 
  // 使用 while 循环进行尾递归消除。
  while (true) {
 
    size_t size = end - begin;

    // Insertion sort is faster for small arrays.
    if (size < insertion_sort_threshold) {
      if (leftmost) {
        insertion_sort(begin, end, comp);
      }
      else {
        unguarded_insertion_sort(begin, end, comp);
      }
      return;
    }

    if(size <= cutoff) {
      std::sort(begin, end, comp);
      return;
    }

    // 选择 作为 3 的中位数或 9 的伪中位数。 Choose pivot as median of 3 or pseudomedian of 9.
    size_t s2 = size >> 1;
    if (size > ninther_threshold) {
      sort3(begin, begin + s2, end - 1, comp);
      sort3(begin + 1, begin + (s2 - 1), end - 2, comp);
      sort3(begin + 2, begin + (s2 + 1), end - 3, comp);
      sort3(begin + (s2 - 1), begin + s2, begin + (s2 + 1), comp);
      std::iter_swap(begin, begin + s2);
    }
    else {
      sort3(begin + s2, begin, end - 1, comp);
    }

    // 如果 *(begin - 1) 是前一个分区操作的右分区的结尾，则 [begin, end) 中没有小于 *(begin - 1) 的元素。 
    // 然后，如果我们的枢轴比较等于 *(begin - 1)，我们改变策略，将相等的元素放入左侧分区，将更大的元素放入右侧分区。
    //  我们不必在左侧分区上递归，因为它已排序（全部相等）。
    if (!leftmost && !comp(*(begin - 1), *begin)) {
      begin = partition_left(begin, end, comp) + 1;
      continue;
    }

    // Partition and get results.
    const auto pair = Branchless ? partition_right_branchless(begin, end, comp) :
                                   partition_right(begin, end, comp);
       
    const auto pivot_pos = pair.first;
    const auto already_partitioned = pair.second;

    // Check for a highly unbalanced partition.
    const size_t l_size = pivot_pos - begin;
    const size_t r_size = end - (pivot_pos + 1);
    const bool highly_unbalanced = l_size < size / 8 || r_size < size / 8;

    // 如果我们得到一个高度不平衡的分区，我们会打乱元素以打破许多模式。
    if (highly_unbalanced) {
      // 如果我们有太多坏 partitions，切换到堆排序以保证 O(n log n) 
      if (--bad_allowed == 0) {
        std::make_heap(begin, end, comp);
        std::sort_heap(begin, end, comp);
        return;
      }

      if (l_size >= insertion_sort_threshold) {
        std::iter_swap(begin, begin + l_size / 4);
        std::iter_swap(pivot_pos - 1, pivot_pos - l_size / 4);
        if (l_size > ninther_threshold) {
          std::iter_swap(begin + 1, begin + (l_size / 4 + 1));
          std::iter_swap(begin + 2, begin + (l_size / 4 + 2));
          std::iter_swap(pivot_pos - 2, pivot_pos - (l_size / 4 + 1));
          std::iter_swap(pivot_pos - 3, pivot_pos - (l_size / 4 + 2));
        }
      }

      if (r_size >= insertion_sort_threshold) {
        std::iter_swap(pivot_pos + 1, pivot_pos + (1 + r_size / 4));
        std::iter_swap(end - 1,     end - r_size / 4);
        if (r_size > ninther_threshold) {
          std::iter_swap(pivot_pos + 2, pivot_pos + (2 + r_size / 4));
          std::iter_swap(pivot_pos + 3, pivot_pos + (3 + r_size / 4));
          std::iter_swap(end - 2,             end - (1 + r_size / 4));
          std::iter_swap(end - 3,             end - (2 + r_size / 4));
        }
      }
    }
    // 平衡得体 
    else {
      // 序列尝试使用插入排序 
      if (already_partitioned &&   partial_insertion_sort(begin, pivot_pos, comp) &&  partial_insertion_sort(pivot_pos + 1, end, comp) ) {
        return;
      }
    }

    // 首先使用递归对左侧分区进行排序，然后对右侧分区进行尾递归消除。
    rt.silent_async(
      [&rt, begin, pivot_pos, comp, bad_allowed, leftmost] () mutable {
        parallel_pdqsort<Iter, Compare, Branchless>( rt, begin, pivot_pos, comp, bad_allowed, leftmost );
      }
    );
    begin = pivot_pos + 1;
    leftmost = false;
  }
}




// ----------------------------------------------------------------------------
// 3-way quick sort
// ----------------------------------------------------------------------------

// 3-way quick sort
template <typename RandItr, typename C>
void parallel_3wqsort(tf::Runtime& rt, RandItr first, RandItr last, C compare) {

  using namespace std::string_literals;

  constexpr auto cutoff = parallel_sort_cutoff<RandItr>();

  sort_partition:

  if(static_cast<size_t>(last - first) < cutoff) {
    std::sort(first, last+1, compare);
    return;
  }

  auto m = pseudo_median_of_nine(first, last, compare);

  if(m != first) {
    std::iter_swap(first, m);
  }

  auto l = first;
  auto r = last;
  auto f = std::next(first, 1);
  bool is_swapped_l = false;
  bool is_swapped_r = false;

  while(f <= r) {
    if(compare(*f, *l)) {
      is_swapped_l = true;
      std::iter_swap(l, f);
      l++;
      f++;
    }
    else if(compare(*l, *f)) {
      is_swapped_r = true;
      std::iter_swap(r, f);
      r--;
    }
    else {
      f++;
    }
  }

  if(l - first > 1 && is_swapped_l) {
    rt.silent_async([&rt, first, l, &compare] () mutable {
      parallel_3wqsort(rt, first, l-1, compare);
    });
  }

  if(last - r > 1 && is_swapped_r) {
    first = r+1;
    goto sort_partition;
  }

  //rt.join();
}





// ----------------------------------------------------------------------------
// tf::Taskflow::sort
// ----------------------------------------------------------------------------

// Function: sort
template <typename B, typename E, typename C>
Task FlowBuilder::sort(B beg, E end, C cmp) {

  Task task = emplace([b=beg, e=end, cmp] (Runtime& rt) mutable {

    using B_t = std::decay_t<unwrap_ref_decay_t<B>>;
    using E_t = std::decay_t<unwrap_ref_decay_t<E>>;

    // 获取迭代器值
    B_t beg = b;
    E_t end = e;

    if(beg == end) {
      return;
    }

    size_t W = rt._executor.num_workers();
    size_t N = std::distance(beg, end);

    // only myself - no need to spawn another graph
    if(W <= 1 || N <= parallel_sort_cutoff<B_t>()) {
      std::sort(beg, end, cmp);
      return;
    }

    //parallel_3wqsort(rt, beg, end-1, cmp);
    parallel_pdqsort<B_t, C,
      is_std_compare_v<std::decay_t<C>> &&
      std::is_arithmetic_v<typename std::iterator_traits<B_t>::value_type>
    >(rt, beg, end, cmp, log2(end - beg));

    rt.join();
  });

  return task;
}

// Function: sort
template <typename B, typename E>
Task FlowBuilder::sort(B beg, E end) {
  using value_type = std::decay_t<decltype(*std::declval<B>())>;
  return sort(beg, end, std::less<value_type>{});
}

}  // namespace tf ------------------------------------------------------------

