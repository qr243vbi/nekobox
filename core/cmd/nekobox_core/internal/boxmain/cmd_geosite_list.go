package boxmain

func GeositeList(path string) ([]string, error) {
	if err := geositePreRun(path); err != nil {
		return nil, err
	}

	return geositeCodeList, nil
}
