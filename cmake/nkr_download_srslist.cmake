
function(download_srslist)
	set(FILE_URL "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json")
	set(DEST_FILE "${CMAKE_SOURCE_DIR}/srslist.json")
	if(NOT EXISTS "${DEST_FILE}")
		message(STATUS "srslist.json not found in source dir, downloading: ${DEST_FILE}")
		file(DOWNLOAD
			"${FILE_URL}"
			"${DEST_FILE}"
			SHOW_PROGRESS
			STATUS download_status
			LOG download_log
		)
		list(GET download_status 0 status_code)
		if(NOT status_code EQUAL 0)
			message(STATUS
				"Download failed: ${download_status}\n${download_log}"
			)
		endif()
	else()
		message(STATUS "File already exists: ${DEST_FILE}")
	endif()
endfunction()
