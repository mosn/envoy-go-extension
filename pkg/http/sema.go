package http

import (
	"sync"
	"sync/atomic"
)

var (
	semaId   uint64
	semaLock sync.Mutex
	semas    = map[uint64]*sync.WaitGroup{}
)

func addWg() (*sync.WaitGroup, uint64) {
	id := atomic.AddUint64(&semaId, 1)

	wg := &sync.WaitGroup{}
	wg.Add(1)

	semaLock.Lock()
	semas[id] = wg
	semaLock.Unlock()

	return wg, id
}

func deleteWg(id uint64) {
	semaLock.Lock()
	delete(semas, id)
	semaLock.Unlock()
}

func getWg(id uint64) *sync.WaitGroup {
	sema := semas[id]
	semaLock.Lock()
	delete(semas, id)
	semaLock.Unlock()
	return sema
}
