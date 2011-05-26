# -*- coding: utf-8 -*-

from setuptools import setup, find_packages

long_desc = '''
This package contains the wikir Sphinx extension.
'''

requires = ['Sphinx>=0.6','wikir']

setup(
    name='wikibuilder',
    version='0.1',
    license='BSD',
    author='D13',
    author_email='dainan13@gmail.com',
    description='Sphinx extension builder for wikir',
    long_description=long_desc,
    zip_safe=False,
    platforms='any',
    packages=find_packages(),
    include_package_data=True,
    install_requires=requires,
    py_modules=['wikibuilder'],
)