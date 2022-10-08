package http

import "unsafe"

type HttpCgoAPI interface {
	HttpContinue(r unsafe.Pointer, status uint64)

	HttpGetHeader(r unsafe.Pointer, key *string, value *string)
	HttpCopyHeaders(r unsafe.Pointer, num uint64, bytes uint64) map[string]string

	HttpSetHeader(r unsafe.Pointer, key *string, value *string)

	HttpGetBuffer(r unsafe.Pointer, bufferPtr uint64, value *string, length uint64)
	HttpSetBuffer(r unsafe.Pointer, bufferPtr uint64, value string)

	HttpFinalize(r unsafe.Pointer, reason int)
}

var httpCgoAPI HttpCgoAPI

func SetCgoAPI(api HttpCgoAPI) {
	httpCgoAPI = api
}

func GetCgoAPI() HttpCgoAPI {
	return httpCgoAPI
}
