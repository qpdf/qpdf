# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For
# a full list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
#
# To see the default sample conf.py, run sphinx-quickstart in an empty
# directory. Most of the original comments and options were removed.
import sphinx_rtd_theme  # noQA F401
import os
import sys

sys.path.append(os.path.abspath("./_ext"))

project = 'QPDF'
copyright = '2005-2021 Jay Berkenbilt, 2022-2025 Jay Berkenbilt and Manfred Holger'
author = 'Jay Berkenbilt'
here = os.path.dirname(os.path.realpath(__file__))
with open(f'{here}/../CMakeLists.txt') as f:
    for line in f.readlines():
        if line.strip().startswith('VERSION '):
            release = line.replace('VERSION', '').strip()
            break
version = release
extensions = [
    'sphinx_rtd_theme',
    'qpdf',
]
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    "body_max_width": None,
}
html_logo = '../logo/qpdf.svg'
html_static_path = ['_static']
html_css_files = [
    'css/wraptable.css',
]
latex_elements = {
    'preamble': r'''
\sphinxDUC{2264}{$\leq$}
\sphinxDUC{2265}{$\geq$}
\sphinxDUC{03C0}{$\pi$}
''',
}
highlight_language = 'none'
