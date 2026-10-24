# sample_aoa_capture 测试说明：

首先看下usage：
====
Usage: ./sample_aoa_capture [OPTION]...

  -h --help             help
  -a --algo-config      specify the config file of backend VQE algorithm
  -r --rate             sample rate and only support 16000(default) or 8000 (Hz)
  -S --suspend-thresold the running frame threshold for automatically triggering sleep.
                        if the frame rate is 16ms, this value is set to 450.
                        each time the system wakes up, it will automatically
                        enter sleep after 4.8s.
  -p --period-size      the period size of per-frame (default: 256)
  -i --in-file          specify in file for offline testing
  -o --out-file         specify output file (default: /tmp/dump_sample.pcm)
  -m --mode             0: AOR witout firmware and output pcm is 1ch
                        1: AAD with firmware and output pcm is 2ch with event
  -f --firmware-path    the firmware for AAD mode
  -w --wakeup-mode      need backend VQE wakeup algrithm,
                        0: ignore wakeup, 1: wakeup, 2: wakeup + command
  -z --print-debug      print debug infos

For example:
  running on AOR mode:
    ./sample_aoa_capture -m 0 -o /tmp/my_aor.pcm
  running on AAD mode and enter suspend after 450 frames:
    ./sample_aoa_capture -m 1 -f /oem/fw_aad_aivad.bin -S 450
====

以上测试默认都不带后端VQE算法。若需要走后端VQE算法，需要加 -a 参数，并且推送
librkaudio_common.so
librkaudio_vqe.so
到/usr/lib/目录，或环境变量LD_LIBRARY_PATH指定加载的绝对路径。例如：
/tmp/sample_aoa_capture_share -m 1 -f /oem/fw_aad_aivad.bin -a /oem/config_aivqe.json

用户也可根据sample_aoa_capture.c的源码自行使用自定义后端的算法，来替换示例中的effect vqe相关处理流程。
