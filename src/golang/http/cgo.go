package http

type HttpCgoAPI interface {
	HttpContinue(filter uint64, status uint64)

	HttpGetRequestHeader(filter uint64, key *string, value *string)
	HttpCopyRequestHeaders(filter uint64, num uint64, bytes uint64) map[string]string

	// HttpEncodeContinue(filter uint64, end int)

	HttpGetResponseHeader(filter uint64, key *string, value *string)
	HttpCopyResponseHeaders(filter uint64, num uint64, bytes uint64) map[string]string
	HttpSetResponseHeader(filter uint64, key *string, value *string)
	HttpGetBuffer(filter uint64, bufferPtr uint64, value *string, length uint64)
	HttpSetBuffer(filter uint64, bufferPtr uint64, value string)
}

var httpCgoAPI HttpCgoAPI

func SetCgoAPI(api HttpCgoAPI) {
	httpCgoAPI = api
}

func GetCgoAPI() HttpCgoAPI {
	return httpCgoAPI
}
