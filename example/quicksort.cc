#include <cstdio>
#include <vector>
#include <algorithm>

template <class ForwardIt>
void quicksort(ForwardIt first, ForwardIt last) {
  if (first == last) {
    return;
  }

  auto pivot = *std::next(first, std::distance(first, last) / 2);
  auto mid1 = std::partition(first, last,
                             [pivot](const auto& em) { return em < pivot; });
  auto mid2 = std::partition(mid1, last,
                             [pivot](const auto& em) { return !(pivot < em); });

  quicksort(first, mid1);
  quicksort(mid2, last);
}

int main() {
  std::vector<int> v = {3, 1, 2};
  quicksort(begin(v), end(v));
  printf("%s\n", std::is_sorted(begin(v), end(v)) ? "works" : "does not work");
}