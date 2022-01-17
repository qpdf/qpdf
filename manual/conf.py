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
copyright = '2005-2021, Jay Berkenbilt'
author = 'Jay Berkenbilt'
# make_dist and the CI build lexically find the release version from this file.
release = '10.5.0'
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
highlight_language = 'none'
