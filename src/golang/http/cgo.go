package http

type HttpCgoAPI interface {
	HttpDecodeContinue(filter uint64, end int)
	HttpGetRequestHeader(filter uint64, key *string, value *string)
}

var httpCgoAPI HttpCgoAPI

func SetCgoAPI(api HttpCgoAPI) {
	httpCgoAPI = api
}

func GetCgoAPI() HttpCgoAPI {
	return httpCgoAPI
}
