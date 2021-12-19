# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For
# a full list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
#
# To see the default sample conf.py, run sphinx-quickstart in an empty
# directory. Most of the original comments and options were removed.
import sphinx_rtd_theme  # noQA F401

project = 'QPDF'
copyright = '2005-2021, Jay Berkenbilt'
author = 'Jay Berkenbilt'
release = '10.4.0'
version = release
extensions = [
    'sphinx_rtd_theme',
]
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    "body_max_width": None,
}
html_logo = '../logo/qpdf.svg'
highlight_language = 'none'
