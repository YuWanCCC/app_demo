# sample_aoa_capture test description:

First look at usage:
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

The above tests do not have the backend VQE algorithm by default. To use the backend VQE algorithm, add the -a parameter and push librkaudio_common.so and librkaudio_vqe.so to the /usr/lib/ directory, or specify the absolute path to load using the LD_LIBRARY_PATH environment variable. For example:
/tmp/sample_aoa_capture_share -m 1 -f /oem/fw_aad_aivad.bin -a /oem/config_aivqe.json

Users can also customize the backend algorithm based on the source code in sample_aoa_capture.c to replace the effect VQE processing flow in the example.