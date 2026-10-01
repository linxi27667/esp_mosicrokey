"""Generate upstream-compatible language declarations into the build tree."""
import json
import sys
from pathlib import Path

assets, output = map(Path, sys.argv[1:])
strings = json.loads((assets / 'locales/en-US/language.json').read_text(encoding='utf-8'))['strings']
strings.update(json.loads((assets / 'locales/zh-CN/language.json').read_text(encoding='utf-8'))['strings'])
sounds = {p.stem for folder in ['locales/en-US', 'locales/zh-CN', 'common'] for p in (assets / folder).glob('*.ogg')}
lines = ['#pragma once', '#include <string_view>', '#define zh_cn', 'namespace Lang {',
         'constexpr const char* CODE = "zh-CN";', 'namespace Strings {']
lines += [f'constexpr const char* {key.upper()} = {json.dumps(value, ensure_ascii=False)};' for key, value in sorted(strings.items())]
lines += ['}', 'namespace Sounds {']
for name in sorted(sounds):
    lines += [f'extern const char audio_{name}_start[] asm("_binary_{name}_ogg_start");',
              f'extern const char audio_{name}_end[] asm("_binary_{name}_ogg_end");',
              f'inline const std::string_view OGG_{name.upper()}{{audio_{name}_start, static_cast<size_t>(audio_{name}_end - audio_{name}_start)}};']
lines += ['}', '}']
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('\n'.join(lines) + '\n', encoding='utf-8')
