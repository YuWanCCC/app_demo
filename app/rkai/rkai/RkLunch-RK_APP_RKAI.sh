
echo "Start app"
sh /oem/usr/ko/insmod_ko.sh

if grep -q 'ID=.*debian' /etc/os-release; then
	exit 0
fi

cfg_file=`find /usr /oem -name rkai_cfg.ini`
rkllm_file=`find /usr /oem -name *.rkllm`
if [ -f "$cfg_file" ];then
	if [ -z "$rkllm_file" ]; then
		echo "There is no rkllm model in /oem/usr/share/app_conf/models/"
		echo "Please get model from https://www.modelscope.cn/models/cdd12138/Qwen3-0.6B-w4a16-rkllm/resolve/master/Qwen3-0.6B_W4A16_RV1126B.rkllm"
	else
		echo "Run LLM"
		rkai_LLM_Demo --config $cfg_file
	fi

	# echo "Run VLM"
	# rkai_VLM_Demo --config $cfg_file
fi
