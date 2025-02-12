package boxmain

func ListGeoip(path string) ([]string, error) {
	if err := geoipPreRun(path); err != nil {
		return nil, err
	}

	return geoipReader.Metadata.Languages, nil
}
