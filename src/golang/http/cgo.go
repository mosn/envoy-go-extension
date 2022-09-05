package http

type HttpCgoAPI interface {
	HttpDecodeContinue(filter uint64, end int)
}

var httpCgoAPI HttpCgoAPI

func SetCgoAPI(api HttpCgoAPI) {
	httpCgoAPI = api
}

func GetCgoAPI() HttpCgoAPI {
	return httpCgoAPI
}
