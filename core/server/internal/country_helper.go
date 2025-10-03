package internal

import "github.com/biter777/countries"

var countryMap map[string]*countries.Country

func init() {
	countryMap = make(map[string]*countries.Country)
	allCountries := countries.AllInfo()
	for _, country := range allCountries {
		countryMap[country.Name] = country
	}
}

func GetCountryByName(name string) *countries.Country {
	return countryMap[name]
}

func GetCountryEmojiByName(name string) string {
	country, ok := countryMap[name]
	if !ok {
		return ""
	}
	return country.Emoji
}
