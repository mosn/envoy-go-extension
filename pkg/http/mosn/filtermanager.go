package mosn

import (
	"context"

	"mosn.io/mosn/pkg/streamfilter"
)

func CreateStreamFilterChain(ctx context.Context, filterChainName string) *streamfilter.DefaultStreamFilterChainImpl {
	fm := streamfilter.GetDefaultStreamFilterChain()

	streamFilterFactory := streamfilter.GetStreamFilterManager().GetStreamFilterFactory(filterChainName)
	if streamFilterFactory != nil {
		streamFilterFactory.CreateFilterChain(ctx, fm)
	}

	return fm
}

func DestroyStreamFilterChain(fm *streamfilter.DefaultStreamFilterChainImpl) {
	if fm != nil {
		streamfilter.PutStreamFilterChain(fm)
	}
}
