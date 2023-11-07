#!/usr/bin/env python

import os
import platform
import re
import sys
from setuptools import find_packages, setup, Extension

# We need at least one extension so that cibuildwheel considers this to be a platform-specific wheel
dummy_extension = Extension(
     'hello',
     sources=['llmware/hello.c']
)

VERSION_FILE = "llmware/__init__.py"
with open(VERSION_FILE) as version_file:
    match = re.search(r"^__version__ = ['\"]([^'\"]*)['\"]",version_file.read(), re.MULTILINE)

if match:
    version = match.group(1)
else:
    raise RuntimeError(f"Unable to find version string in {VERSION_FILE}.")

with open("README.md") as readme_file:
    long_description = readme_file.read()

setup(
    name="llmware",  # Required
    version=version,  # Required
    description="An enterprise-grade LLM-based development framework, tools, and fine-tuned models",  # Optional
    long_description=long_description,  # Optional
    long_description_content_type="text/markdown",  # Optional
    url="https://github.com/llmware-ai",
    project_urls={
        'Repository': 'https://github.com/llmware-ai/llmware',
    },
    author="llmware",
    author_email="support@aibloks.com",  # Optional
    classifiers=[  # Optional
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Software Development",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
    ],
    keywords="ai,data,development",  # Optional 
    packages=['llmware'],
    package_data={'llmware': ['*.so', '*.dylib']},
    python_requires=">=3.9, <3.11",
    zip_safe=True,
    ext_modules=[dummy_extension],
    install_requires=[
        'ai21>=1.0.3',
        'anthropic>=0.3.11',
        'beautifulsoup4>=4.11.1',
        'boto3>=1.24.53',
        'cohere>=4.1.3',
        'faiss-cpu>=1.7.4',
        'google-cloud-aiplatform>=1.33.1',
        'lxml>=4.9.3',
        'numpy>=1.23.2',
        'openai>=0.27.7',
        'pandas>=2.1.1',
        'pdf2image>=1.16.0',
        'Pillow>=10.0.1',
        'pymilvus>=2.3.0',
        'pymongo>=4.5.0',
        'pytesseract>=0.3.10',
        'python-on-whales>=0.64.3',
        'scipy>=1.11.2',
        'scikit-learn>=1.3.1',
        'tokenizers>=0.13.3',
        'torch>=1.13.1',
        'Werkzeug>=3.0.1',
        'word2number>=1.1',
        'Wikipedia-API>=0.6.0',
        'yfinance>=0.2.28'
    ]
)
