#!/usr/bin/env python

import os
import platform
import re
import sys
from setuptools import find_packages, setup, Extension
# from setuptools.command.install import install
# from setuptools.command.develop import develop
# from setuptools.command.egg_info import egg_info

#!/usr/bin/env python

# Native extensions
native_graph = Extension(
    'llmware.graph',
    sources=['../llmware-native/graph_llmware.c'],
    libraries=['mongoc-1.0', 'bson-1.0', 'xml2', 'png', 'zip', 'poppler', 'tesseract'],
    include_dirs=['.','/opt/homebrew/Cellar/mongo-c-driver/1.24.4/include/libbson-1.0','/opt/homebrew/Cellar/mongo-c-driver/1.24.4/include/libmongoc-1.0'],
    library_dirs=['/usr/local/lib']
)
native_pdf = Extension(
    'llmware.pdf',
    sources=['../llmware-native/pdf_parser_llmware.c'],
    libraries=['mongoc-1.0', 'bson-1.0', 'xml2', 'png', 'zip', 'poppler', 'tesseract'],
    include_dirs=['.','/opt/homebrew/Cellar/mongo-c-driver/1.24.4/include/libbson-1.0','/opt/homebrew/Cellar/mongo-c-driver/1.24.4/include/libmongoc-1.0'],
    library_dirs=['/usr/local/lib']
)
native_office = Extension(
    'llmware.office',
    sources=['../llmware-native/office_llmware.c'],
    libraries=['mongoc-1.0', 'bson-1.0', 'xml2', 'png', 'zip', 'poppler', 'tesseract'],
    include_dirs=['.','/opt/homebrew/Cellar/mongo-c-driver/1.24.4/include/libbson-1.0','/opt/homebrew/Cellar/mongo-c-driver/1.24.4/include/libmongoc-1.0'],
    library_dirs=['/usr/local/lib']
)


# def custom_install_command():

#     try:
#         if platform.system() == "Windows":
#             print("llmware is not yet supported on Windows, but it's on our roadmap.  Check back soon!")
#             sys.exit(-1)

#         if platform.system() == "Darwin":
#             if os.system('brew --version') != 0:
#                 error_message="llmware needs Homebrew ('brew') to be installed to setup a few depencencies."
#                 error_message+="\nInstalling HomeBrew is quick and easy: https://brew.sh"
#                 sys.exit(error_message)
#             os.system('brew install mongo-c-driver libpng libzip libtiff zlib tesseract poppler')
#             return
    
#         if platform.system() == "Linux":
#             if os.system('apt list') == 0:
#                 os.system('apt update && apt install -y gcc libxml2 libpng-dev libmongoc-dev libzip4 tesseract-ocr poppler-utils')
#                 return
#     except Exception as e:
#         print (e)
#         # Silently exit (and allow the install to continue if there was any problem)


# class CustomInstallCommand(install):
#     def run(self):
#         custom_install_command()
#         install.run(self)
        
# class CustomDevelopCommand(develop):
#     def run(self):
#         custom_install_command()
#         develop.run(self)

# class CustomEggInfoCommand(egg_info):
#     def run(self):
#         custom_install_command()
#         egg_info.run(self)

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
    package_data={'llmware': ['lib/**/**/*.so', 'lib/**/**/*.dylib']},
    python_requires=">=3.9, <3.11",
    zip_safe=True,
    ext_modules=[native_graph, native_pdf, native_office],
    # cmdclass={
    #     'install': CustomInstallCommand,
    #     'develop': CustomDevelopCommand,
    #     'egg_info': CustomEggInfoCommand,
    # },
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
        'pdf2image>=1.16.0',
        'Pillow>=9.2.0',
        'pymilvus>=2.3.0',
        'pymongo>=4.5.0',
        'pytesseract>=0.3.10',
        'python-on-whales>=0.64.3',
        'scipy>=1.11.2',
        'tokenizers>=0.13.3',
        'torch>=1.13.1',
        'Werkzeug>=2.3.7',
        'word2number>=1.1',
        'Wikipedia-API>=0.6.0',
        'yfinance>=0.2.28'
    ]
)
