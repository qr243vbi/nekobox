package vmess

type ServiceOption func(service *Service[string])

func ServiceWithTimeFunc(timeFunc TimeFunc) ServiceOption {
	return func(service *Service[string]) {
		service.time = timeFunc
	}
}

func ServiceWithDisableHeaderProtection() ServiceOption {
	return func(service *Service[string]) {
		service.disableHeaderProtect = true
	}
}
