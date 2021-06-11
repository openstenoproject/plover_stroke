#!/usr/bin/env python3

from setuptools import Extension, setup


setup(
    ext_modules=[
        Extension('_plover_stroke',
                  sources=['_plover_stroke.c']),
    ],
)
