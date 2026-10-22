import ctypes
import json
import os
import sys

assert sys.platform == 'win32'
result = {'platform': sys.platform, 'system_acp': ctypes.windll.kernel32.GetACP(),
          'console_input_cp': ctypes.windll.kernel32.GetConsoleCP(),
          'console_output_cp': ctypes.windll.kernel32.GetConsoleOutputCP(),
          'CL': os.environ.get('CL'), '_CL_': os.environ.get('_CL_')}
print(json.dumps(result))
assert result['system_acp'] == 936
assert not result['CL'] and not result['_CL_']
