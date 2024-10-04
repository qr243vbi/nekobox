package boxmain

func GeositeList() ([]string, error) {
	if err := geositePreRun(); err != nil {
		return nil, err
	}

	return geositeCodeList, nil
}
