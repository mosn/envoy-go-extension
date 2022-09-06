package utils

import "C"
import (
	"reflect"
	"unsafe"
)

type CStyleString C.char
type CStyleULongLong C.ulonglong

func BytesToString(ptr uint64, len uint64) string {
	var s string
	var sHdr = (*reflect.StringHeader)(unsafe.Pointer(&s))
	sHdr.Data = uintptr(unsafe.Pointer(uintptr(ptr)))
	sHdr.Len = int(len)
	return s
}

func BytesToSlice(ptr uint64, len uint64) []byte {
	var s []byte
	var sHdr = (*reflect.SliceHeader)(unsafe.Pointer(&s))
	sHdr.Data = uintptr(unsafe.Pointer(uintptr(ptr)))
	sHdr.Len = int(len)
	sHdr.Cap = int(len)
	return s
}
