#!/usr/bin/env python3
"""Cross-platform static check of the embedded LJuno-116 Help and About metadata."""
from pathlib import Path
from html.parser import HTMLParser

root = Path(__file__).resolve().parents[1]
help_html = (root / 'Help/LJuno116Help.html').read_text(encoding='utf-8')
editor = (root / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
workflow = (root / '.github/workflows/build-windows.yml').read_text(encoding='utf-8')

class ManualHTML(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.languages = []
        self.links = []
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == 'article' and 'manual' in a.get('class','').split():
            self.languages.append(a.get('id'))
        if tag == 'a' and a.get('href','').startswith('#'):
            self.links.append(a['href'][1:])

html = ManualHTML()
html.feed(help_html)
assert html.languages == ['en','it','es','pt','fr','ru','zh','ja'], html.languages
assert set(html.languages).issubset(set(html.links))
assert help_html.count('0.99.55') >= 16
for lang in html.languages:
    assert f'id="{lang}"' in help_html
for marker in ['27 October 2026', '27 ottobre 2026', '27 de octubre de 2026',
               '27 de outubro de 2026', '27 octobre 2026', '27 октября 2026',
               '2026 年 10 月 27 日', '2026年10月27日']:
    assert marker in help_html, marker
for feature in ['Alt+Q', 'Alt+S', 'Alt+M', 'Ctrl+A', 'Ctrl+C', 'Ctrl+Shift+C/X/V',
                'Alt+B']:
    assert feature in help_html, feature
assert 'ljunoVersion = "0.99.55"' in editor
assert 'ljunoReleaseDate = "27 October 2026"' in editor
assert 'copyToClipboard()' in editor
assert 'VERSION 0.99.55' in cmake
assert 'LJuno-116-0.99.55-TEST106D-Windows-VST3' in workflow
assert (root / '.gitignore').exists()
print('OK: eight languages, Help updates, About 0.99.55 and 27 October 2026')
