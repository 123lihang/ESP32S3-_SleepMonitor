idf_component_register(
    SRC_DIRS 
        "."
        "APP"
		"key_task"
    INCLUDE_DIRS 
        "."
        "APP"
		"key_task"
    REQUIRES 
        BSP
        mqtt
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        bt
        wifi_provisioning
        protocomm
        lwip
    )
 
# 嵌入CA证书文件（路径对应的certs文件夹）
target_add_binary_data(${COMPONENT_TARGET} "certs/emqxsl-ca.pem" TEXT)

