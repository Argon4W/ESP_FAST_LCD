idf_component_register(
		REQUIRES esp_lcd
		SRC_DIRS "./src/fast_lcd/" "./src/fast_text_engine/"
		INCLUDE_DIRS "./include/"
		PRIV_INCLUDE_DIRS "./priv_include/"
)