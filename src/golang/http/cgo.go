package http

type HttpCgoAPI interface {
	HttpContinue(filter uint64, status uint64)

	HttpGetHeader(filter uint64, key *string, value *string)
	HttpCopyHeaders(filter uint64, num uint64, bytes uint64) map[string]string

	HttpSetHeader(filter uint64, key *string, value *string)

	HttpGetBuffer(filter uint64, bufferPtr uint64, value *string, length uint64)
	HttpSetBuffer(filter uint64, bufferPtr uint64, value string)

	HttpFinalize(filter uint64, reason int)
}

var httpCgoAPI HttpCgoAPI

func SetCgoAPI(api HttpCgoAPI) {
	httpCgoAPI = api
}

func GetCgoAPI() HttpCgoAPI {
	return httpCgoAPI
}
