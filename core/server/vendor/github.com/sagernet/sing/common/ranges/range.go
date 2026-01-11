package ranges

import (
	"sort"

	"github.com/sagernet/sing/common/x/constraints"
)

type Range[N comparable] struct {
	Start N
	End   N
}

func New[N constraints.Integer](start N, end N) Range[N] {
	return Range[N]{start, end}
}

func NewSingle[N constraints.Integer](index N) Range[N] {
	return Range[N]{index, index}
}

func Merge[N constraints.Integer](ranges []Range[N]) (mergedRanges []Range[N]) {
	if len(ranges) == 0 {
		return
	}
	sort.Slice(ranges, func(i, j int) bool {
		return ranges[i].Start < ranges[j].Start
	})
	mergedRanges = ranges[:1]
	var rangeIndex N
	for _, r := range ranges[1:] {
		if r.Start > mergedRanges[rangeIndex].End+1 {
			mergedRanges = append(mergedRanges, r)
			rangeIndex++
		} else if r.End > mergedRanges[rangeIndex].End {
			mergedRanges[rangeIndex].End = r.End
		}
	}
	return
}

func Revert[N constraints.Integer](start, end N, ranges []Range[N]) (revertedRanges []Range[N]) {
	if len(ranges) == 0 {
		return
	}
	ranges = Merge(ranges)
	if ranges[0].Start > start {
		revertedRanges = append(revertedRanges, Range[N]{start, ranges[0].Start - 1})
	}
	rangeEnd := ranges[0].End
	for _, r := range ranges[1:] {
		if r.Start > rangeEnd+1 {
			revertedRanges = append(revertedRanges, Range[N]{rangeEnd + 1, r.Start - 1})
		}
		rangeEnd = r.End
	}
	if end > rangeEnd {
		revertedRanges = append(revertedRanges, Range[N]{rangeEnd + 1, end})
	}
	return
}

func Exclude[N constraints.Integer](ranges []Range[N], targetRanges []Range[N]) []Range[N] {
	ranges = Merge(ranges)
	if len(ranges) == 0 {
		return nil
	}
	targetRanges = Merge(targetRanges)
	if len(targetRanges) == 0 {
		return ranges
	}
	var mergedRanges []Range[N]
	rangeStart := ranges[0].Start
	rangeEnd := ranges[0].End
	rangeIndex := rangeStart
	ranges = ranges[1:]
	targetStart := targetRanges[0].Start
	targetEnd := targetRanges[0].End
	targetRanges = targetRanges[1:]
	for {
		if targetStart > rangeEnd {
			if rangeIndex <= rangeEnd {
				mergedRanges = append(mergedRanges, Range[N]{rangeIndex, rangeEnd})
			}
			if len(ranges) == 0 {
				break
			}
			rangeStart = ranges[0].Start
			rangeEnd = ranges[0].End
			rangeIndex = rangeStart
			ranges = ranges[1:]
			continue
		}
		if targetStart > rangeIndex {
			mergedRanges = append(mergedRanges, Range[N]{rangeIndex, targetStart - 1})
			rangeIndex = targetStart + 1
		}
		if targetEnd <= rangeEnd {
			rangeIndex = targetEnd + 1
			if len(targetRanges) == 0 {
				break
			}
			targetStart = targetRanges[0].Start
			targetEnd = targetRanges[0].End
			targetRanges = targetRanges[1:]
		}
	}
	if rangeIndex <= rangeEnd {
		mergedRanges = append(mergedRanges, Range[N]{rangeIndex, rangeEnd})
	}
	return Merge(append(mergedRanges, ranges...))
}
