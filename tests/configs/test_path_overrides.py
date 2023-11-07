

import os

from llmware.configs import LLMWareConfig
from llmware.library import Library
from llmware.retrieval import Query
from llmware.setup import Setup

def test_standard_account_setup():

    library_name = "test_standard_account_setup"
    library = Library().create_new_library(library_name)

    # add files
    sample_files_path = Setup().load_sample_files()
    library.add_files(os.path.join(sample_files_path,"SmallLibrary"))
    
    # run test query
    results = Query(library).text_query("base salary")

    assert len(results) > 0


# Test overriding llmware_data folder
def test_setup_custom_location():

    #   upstream application creating custom home path, e.g., aibloks
    new_home_folder = "llmware_data_custom"
    LLMWareConfig.set_home(os.path.join(LLMWareConfig.get_home(), new_home_folder))

    new_llmware_path = "aibloks_data"
    LLMWareConfig.set_llmware_path_name(new_llmware_path)

    # Make folders if they don't already exist
    if not os.path.exists(LLMWareConfig().get_home()):
        os.mkdir(LLMWareConfig().get_home())
        if not os.path.exists(LLMWareConfig().get_llmware_path()):
            LLMWareConfig.setup_llmware_workspace()

    assert new_home_folder in LLMWareConfig.get_home()
    assert new_home_folder in LLMWareConfig.get_llmware_path() and new_llmware_path in LLMWareConfig.get_llmware_path()
    assert new_home_folder in LLMWareConfig.get_library_path() and new_llmware_path in LLMWareConfig.get_library_path()

    # test with library setup and adding files
    library_name = "library_with_custom_path"
    library = Library().create_new_library(library_name)
    sample_files_path = Setup().load_sample_files()
    library.add_files(os.path.join(sample_files_path,"Agreements"))

    assert new_home_folder in library.library_main_path

    results = Query(library).text_query("salary")

    assert len(results) > 0
  