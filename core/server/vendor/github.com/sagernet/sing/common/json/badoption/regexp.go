package badoption

import (
	"regexp"

	"github.com/sagernet/sing/common/json"
)

type Regexp regexp.Regexp

func (r *Regexp) Build() *regexp.Regexp {
	return (*regexp.Regexp)(r)
}

func (r *Regexp) MarshalJSON() ([]byte, error) {
	return json.Marshal((*regexp.Regexp)(r).String())
}

func (r *Regexp) UnmarshalJSON(content []byte) error {
	var stringValue string
	err := json.Unmarshal(content, &stringValue)
	if err != nil {
		return err
	}
	regex, err := regexp.Compile(stringValue)
	if err != nil {
		return err
	}
	*r = Regexp(*regex)
	return nil
}
