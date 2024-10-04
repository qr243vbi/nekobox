package boxmain

func ListGeoip() ([]string, error) {
	if err := geoipPreRun(); err != nil {
		return nil, err
	}

	return geoipReader.Metadata.Languages, nil
}
