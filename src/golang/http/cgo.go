package http

type HttpCgoAPI interface {
	HttpDecodeContinue(filter uint64, end int)
	HttpGetRequestHeader(filter uint64, key *string, value *string)
	HttpCopyRequestHeaders(filter uint64, num uint64, bytes uint64) map[string]string

	HttpEncodeContinue(filter uint64, end int)
	HttpGetResponseHeader(filter uint64, key *string, value *string)
	HttpCopyResponseHeaders(filter uint64, num uint64, bytes uint64) map[string]string
}

var httpCgoAPI HttpCgoAPI

func SetCgoAPI(api HttpCgoAPI) {
	httpCgoAPI = api
}

func GetCgoAPI() HttpCgoAPI {
	return httpCgoAPI
}
