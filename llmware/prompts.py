
# Copyright 2023 llmware

# Licensed under the Apache License, Version 2.0 (the "License"); you
# may not use this file except in compliance with the License.  You
# may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied.  See the License for the specific language governing
# permissions and limitations under the License.


from bson import ObjectId
import statistics
from collections import Counter
import re
import time
import logging
import os

from llmware.util import Utilities, CorpTokenizer, PromptCatalog, YFinance, Graph
from llmware.resources import PromptState
from llmware.models import ModelCatalog
from llmware.parsers import Parser
from llmware.retrieval import Query
from llmware.library import Library
from llmware.exceptions import LibraryObjectNotFoundException, PromptNotInCatalogException, DependencyNotInstalledException

class Prompt:

    def __init__(self, llm_name=None, tokenizer=None, model_card=None, library=None, account_name="llmware",
                 prompt_id=None, save_state=True, llm_api_key=None, llm_model=None, from_hf=False,
                 prompt_catalog=None, temperature=0.5):

        self.account_name = account_name
        self.library = library

        # model specific attributes
        self.model_card = model_card
        self.tokenizer = tokenizer

        self.llm_model = None
        self.llm_model_api_key = llm_api_key
        self.llm_name = llm_name

        if from_hf and llm_model:
            self.llm_model = ModelCatalog().load_hf_generative_model(llm_model, tokenizer)
            # print("update: loading HF Generative model - ", self.llm_model)

        # default batch size, assuming all LLMs have min 2048 full context (50% in / 50% out)
        self.context_window_size = 1000

        if model_card:

            if "model_name" in model_card:
                self.llm_model = ModelCatalog().load_model(model_card["model_name"], api_key=llm_api_key)
                self.context_window_size = self.llm_model.max_input_len

        # if passed llm model name, it will 'over-ride' the model_card if both passed
        if llm_name:

            self.llm_model = ModelCatalog().load_model(llm_name, api_key=llm_api_key)
            self.context_window_size = self.llm_model.max_input_len

        if not tokenizer:
            self.tokenizer = Utilities().get_default_tokenizer()
        else:
            self.tokenizer = tokenizer

        # inference parameters
        self.temperature = temperature
        self.prompt_type = ""
        self.llm_max_output_len = 200

        # state attributes
        if prompt_id:
            PromptState(self).load_state(prompt_id)
            self.prompt_id = prompt_id
        else:
            new_prompt_id = PromptState(self).issue_new_prompt_id()
            self.prompt_id = PromptState(self).initiate_new_state_session(new_prompt_id)

            logging.info(f"update: creating new prompt id - {new_prompt_id}")
      
        self.save_prompt_state = save_state

        #   interaction_history is the main running 'active' tracker of current prompt history
        #   interaction_history is added by each 'register' invocation
        #   interaction_history can also be pulled from PromptState, or from database lookup

        self.interaction_history = []

        # dialog tracker is an extract from the interaction history, consisting of running series of tuples:
        #   --"prompt" & "llm_response" response

        self.dialog_tracker = []

        self.llm_state_vars = ["llm_response", "prompt",
                               "instruction", "usage", "time_stamp", "calling_app_ID", "account_name",
                               "prompt_id", "batch_id", "event_type",
                               # source/evidence
                               "evidence", "evidence_metadata", "biblio"
                               # fact-checking
                               "source_review", "comparison_stats", "fact_check",
                               # human-in-the-loop feedback
                               "human_feedback","human_assessed_accuracy", "human_rating", "change_log"]

        # prompt catalog options
        if prompt_catalog:
            self.pc = prompt_catalog
            # print("update: loading custom prompt catalog")

        else:
            self.pc = PromptCatalog()

        self.prompt_catalog = self.pc.get_all_prompts()

        # source materials - available for all prompts, passed as 'context'
        # this is a 'stateful' list that aggregates and tracks all of the source materials added to the prompt
        # each list entry consists of a dict with keys - "batch_id" | "text" | "batch_metadata" | "batch_stats"
        #   --batch_metadata is a list of metadata for each 'sub-source' integrated into the batch
        #   --batch_stats is a sub-list that tracks that # of elements in the batch_metadata

        self.source_materials = []

        self.batch_separator = "\n"

        """
        if self.llm:
            self.batch_separator = self.llm.separator
        """

        self.query_results = None

    # changes in importing huggingface models
    def load_model(self, gen_model,api_key=None, from_hf=False):

        if api_key:
            self.llm_model_api_key = api_key

        if not from_hf:
            self.llm_model = ModelCatalog().load_model(gen_model, api_key=self.llm_model_api_key)
        else:
            try:
                # will wrap in Exception if import fails and move to model catalog class
                from transformers import AutoModelForCausalLM, AutoTokenizer
            except:
                raise DependencyNotInstalledException("transformers")

            if api_key:
                # may look to add further settings/configuration in the future for hf models, e.g., trust_remote_code
                custom_hf_model = AutoModelForCausalLM.from_pretrained(gen_model,token=api_key, trust_remote_code=True)
                hf_tokenizer = AutoTokenizer.from_pretrained(gen_model,token=api_key)
            else:
                custom_hf_model = AutoModelForCausalLM.from_pretrained(gen_model)
                hf_tokenizer = AutoTokenizer.from_pretrained(gen_model)

            #   now, we have 'imported' our own custom 'instruct' model into llmware
            self.llm_model = ModelCatalog().load_hf_generative_model(custom_hf_model, hf_tokenizer,
                                                                     instruction_following=False,
                                                                     prompt_wrapper="human_bot")
            # prepare 'safe name' without file paths
            self.llm_model.model_name = re.sub("[/]","---",gen_model)

        self.llm_name = gen_model
        self.context_window_size = self.llm_model.max_input_len

        return self


    def set_inference_parameters(self, temperature=0.5, llm_max_output_len=200):
        self.temperature = temperature
        self.llm_max_output_len = llm_max_output_len
        return self

    def get_current_history(self, key_list=None):

        # will return selected state vars from current prompt session, based on key list

        if not key_list:
            key_list = self.llm_state_vars

        output_dict = {}
        for i, keys in enumerate(key_list):
            output_dict.update({keys: []})
            for j, entries in enumerate(self.interaction_history):
                if keys in entries:
                    output_dict[keys].append(entries[keys])

        return output_dict

    def clear_history(self):
        # removes elements from interaction history
        self.interaction_history = []
        self.dialog_tracker = []
        return self

    def clear_source_materials(self):
        self.source_materials = []
        return self

    def register_llm_inference (self, ai_dict, prompt_id=None, trx_dict=None):

        if not prompt_id:
            prompt_id = self.prompt_id

        # update elements from interaction
        ai_dict.update({"prompt_id": prompt_id})
        ai_dict.update({"event_type": "inference"})
        ai_dict.update({"human_feedback": ""})
        ai_dict.update({"human_assessed_accuracy": ""})

        # if trx_dict passed -> append key/value pairs into ai_dict
        if isinstance(trx_dict, dict):
            for key,value in trx_dict.items():
                ai_dict.update({key:value})

        # captures new interaction into the interaction history
        logging.info("update: ai_dict getting registered - %s", ai_dict["event_type"])

        PromptState(self).register_interaction(ai_dict)
        new_dialog = {"user": ai_dict["prompt"], "bot": ai_dict["llm_response"]}
        self.dialog_tracker.append(new_dialog)

        return ai_dict

    def lookup_llm_trx_all (self):
        ai_trx_list = PromptState(self).full_history()
        return ai_trx_list

    def load_state(self, prompt_id, clear_current_state=True):
        PromptState(self).load_state(prompt_id,clear_current_state=clear_current_state)
        for entries in self.interaction_history:
            self.dialog_tracker.append({"user": entries["prompt"], "bot": entries["llm_response"]})

        return self

    def save_state(self):
        PromptState(self).save_state(self.prompt_id)
        return self

    def lookup_by_prompt_id (self, prompt_id):
        ai_trx_list = PromptState(self).lookup_by_prompt_id(prompt_id)
        return ai_trx_list

    def lookup_ai_trx_with_filter(self, filter_dict):
        ai_trx_list = PromptState(self).lookup_prompt_with_filter(filter_dict)
        return ai_trx_list

    # prepare sources

    def add_source_new_query(self, library, query=None, query_type="semantic", result_count=10):

        # step 1 - run selected query against library
        query_results = Query(library).query(query,query_type=query_type, result_count=result_count, results_only=True)

        # step 2 - package query_results directly as source, loaded to prompt, and packaged as 'llm context'
        sources = Sources(self).package_source(query_results,aggregate_source=True)

        # enables use of 'prompt_with_sources'

        return sources

    def add_source_query_results(self, query_results):

        #       example use - run a query directly, and then 'add' the query results to a prompt
        #       query_results = Query(self.library).semantic_query("what is the duration of the non-compete clause?")
        #       prompter = Prompt().load_model("claude-instant-v1",api_key="my_api_key")
        #       sources = prompter.add_source_query_results(query_results["results"])

        sources = Sources(self).package_source(query_results,aggregate_source=True)

        return sources

    def add_source_library(self, library_name):

        #       example use - created a small library with a few key documents in a previous step
        #       my_lib.add_documents(fp)
        #       sources = prompter.add_source_library("my_lib")

        lib = Library().load_library(library_name)
        query_results = Query(lib).get_whole_library()

        sources = Sources(self).package_source(query_results,aggregate_source=True)

        return sources

    def add_source_wikipedia(self, topic, article_count=3, query=None):

        # step 1 - get wikipedia article
        output = Parser().parse_wiki([topic],write_to_db=False,target_results=article_count)

        if query:
            output = Utilities().fast_search_dicts(query, output, remove_stop_words=True)

        for i, entries in enumerate(output):
            logging.info("update: source entries - %s - %s", i, entries)

        # step 2 - package wiki article results as source, loaded to prompt, and packaged as 'llm context'
        sources = Sources(self).package_source(output,aggregate_source=True)

        return sources

    def add_source_yahoo_finance(self, ticker=None, key_list=None):

        #   example:    primary use is to quickly grab a factset about a specific company / stock ticker
        #               and 'inject' real-time, up-to-date fact set into the prompt to minimize hallucination risk

        fin_info = YFinance().ticker(ticker).info

        logging.info("update: fin_info - %s ", fin_info)

        output = ""
        if key_list:
            for keys in key_list:
                if keys in fin_info:
                    output += keys + " : " + str(fin_info[keys]) + self.batch_separator
        else:
            for keys, values in fin_info.items():
                output += keys + " : " + str(values) + self.batch_separator

        results = {"file_source": "yfinance-" + str(ticker), "page_num": "na", "text": output}

        logging.info("update: yfinance results - %s ", results)

        # step 2 - package as source
        sources = Sources(self).package_source([results], aggregate_source=True)

        return sources

    # TODO - WIP - how to pass/add knowledge graph context to a prompt
    def add_source_knowledge_graph(self, library, query):

        # need to check for library and for graph
        if library:
            self.library = library

        if not self.library:
            raise LibraryObjectNotFoundException

        if self.library.get_knowledge_graph_status() == "yes":

            kg_output = Graph(self.library).kg_query(query,th=10)
            text_string_out = ""

            for key, values in kg_output.items():
                if key:
                    text_string_out += key + " "
                    for entries in values:
                        text_string_out += entries + " "

            # print("update: kg_output - ", kg_output, text_string_out)
            source_output = [{"text": text_string_out, "page_num":0, "file_source": "knowledge_graph"}]

            sources = Sources(self).package_source(source_output, aggregate_source=True)
        else:
            raise LibraryObjectNotFoundException

        return sources

    def add_source_website(self, url, query=None):

        # get website content
        output = Parser().parse_website(url,write_to_db=False,max_links=3)

        if query:
            output = Utilities().fast_search_dicts(query, output, remove_stop_words=True)

        """
        for i, entry in enumerate(output):
            logging.info("update: website parse - %s - %s", i, entry)
        """

        sources = Sources(self).package_source(output, aggregate_source=True)

        return sources

    def add_source_document(self, input_fp,input_fn, query=None):

        #   example:  intended for use to rapidly parse and add a document (of any type) from local file to a prompt

        output = Parser().parse_one(input_fp,input_fn)

        # run in memory filtering to sub-select from document only items matching query
        if query:
            output = Utilities().fast_search_dicts(query, output, remove_stop_words=True)

        """
        for i, entry in enumerate(output):
            print("update: results - ", i, len(entry["text"]),entry)
            # logging.info("update: parsing output - %s - %s ", i, entry)
        """

        sources = Sources(self).package_source(output, aggregate_source=True)

        return sources

    def add_source_last_interaction_step(self):

        # will take the last interaction and add to source, useful in conversational dialog

        interaction= ""
        if len(self.dialog_tracker) > 0:
            interaction += self.dialog_tracker[-1]["user"] + "\n" + self.dialog_tracker[-1]["bot"] + "\n"

        interaction_source = [{"text": interaction, "page_num":0, "file_source":"dialog_tracker"}]

        # print("interaction_source - ", interaction_source)

        sources = Sources(self).package_source(interaction_source, aggregate_source=True)

        return sources

    def review_sources_summary(self):

        #   Source metadata for each entry -    ["batch_id", "text", "metadata", "biblio", "batch_stats",
        #                                       "batch_stats.tokens", "batch_stats.chars", "batch_stats.samples"]

        source_summary_output = []
        for i, sources in enumerate(self.source_materials):
            new_entry = {"batch_id": sources["batch_id"], "batch_stats": sources["batch_stats"]}
            source_summary_output.append(new_entry)

        return source_summary_output

    def prompt_with_source(self, prompt, prompt_name=None, source_id_list=None, first_source_only=True,
                           max_output=None, temperature=None):

        #   this method is intended to be used in conjunction with sources as follows:
        #           prompter = Prompt().load_model("claude-instant-v1", api_key=None)
        #           source = prompter.add_source (....)
        #           response = prompter.prompt_with_source("what is the stock price of XYZ?")
        #
        #   if multiple loaded sources, then the method will automatically call the model several times
        #       --user can select either 'call once' with first_source_only = True
        #       --OR ... by selecting specific sources by their batch_id,
        #               e.g., source_id_list = [0,1,5] would iterate through sources 0, 1, 5

        response_list = []
        response_dict = {}

        if prompt_name:
            self.prompt_type = prompt_name

        if max_output:
            self.llm_max_output_len = max_output
        
        if temperature:
            self.temperature = temperature
            
        #   this method assumes a 'closed context' with set of preloaded sources into the prompt
        if len(self.source_materials) == 0:
            logging.error("error:  to use prompt_with_source, there must be a loaded source - try '.add_sources' first")
            return [{}]

        #   this is the 'default' and will use the first batch of source material only
        if first_source_only:

            response_dict = self.prompt_main(prompt,prompt_name=self.prompt_type,
                                             context=self.source_materials[0]["text"],
                                             register_trx=False, temperature=temperature)

            # add details on the source materials to the response dict
            if "metadata" in self.source_materials[0]:
                response_dict.update({"evidence_metadata": self.source_materials[0]["metadata"]})

            if "biblio" in self.source_materials[0]:
                response_dict.update({"biblio": self.source_materials[0]["biblio"]})

            response_list.append(response_dict)

        else:
            # if first_source_only is false, then run prompts with all of the sources available
            for i, batch in enumerate(self.source_materials):
                if source_id_list:

                    if i in source_id_list:
                        response_dict = self.prompt_main(prompt,prompt_name=self.prompt_type,
                                                         context=self.source_materials[i]["text"],
                                                         register_trx=False, temperature=temperature)

                        # add details on the source materials to the response dict
                        if "metadata" in self.source_materials[i]:
                            response_dict.update({"evidence_metadata": self.source_materials[i]["metadata"]})

                        if "biblio" in self.source_materials[i]:
                            response_dict.update({"biblio": self.source_materials[i]["biblio"]})

                        response_list.append(response_dict)

                else:

                    response_dict = self.prompt_main(prompt, prompt_name=self.prompt_type,
                                                     context=self.source_materials[i]["text"],
                                                     register_trx=False, temperature=temperature)

                    # add details on the source materials to the response dict
                    if "metadata" in self.source_materials[i]:
                        response_dict.update({"evidence_metadata": self.source_materials[i]["metadata"]})

                    if "biblio" in self.source_materials[i]:
                        response_dict.update({"biblio": self.source_materials[i]["biblio"]})

                    response_list.append(response_dict)

                # log progress of iterations at info level
                logging.info("update: prompt_with_sources - iterating through batch - %s of total %s - %s",
                             i, len(self.source_materials), response_dict)

                logging.info("update: usage stats - %s ", response_dict["usage"])

        # register inferences in state history, linked to prompt_id
        for l, llm_inference in enumerate(response_list):

            logging.info ("update: llm inference - %s - %s - %s", l, len(response_list),llm_inference)

            self.register_llm_inference(llm_inference)

        return response_list

    # prompt management

    def select_prompt_from_catalog(self, prompt_name):

        if prompt_name in self.pc.list_all_prompts():
            self.prompt_type = prompt_name
        else:
            raise PromptNotInCatalogException(prompt_name)

        return self

    def prompt_from_catalog(self, prompt, context=None, prompt_name=None, inference_dict=None):

        if prompt_name not in self.pc.list_all_prompts():
            raise PromptNotInCatalogException(prompt_name)

        # self.llm_model.add_prompt_engineering= prompt_name
        response = self.prompt_main(prompt,context=context, prompt_name=prompt_name,inference_dict=inference_dict)

        return response

    # useful 'out of the box' prompts
    def number_or_none(self, prompt, context=None):
        # rename - "facts_or_not_found"
        output = self.prompt_from_catalog(prompt, context=context,prompt_name="number_or_none")
        return output

    def summarize_with_bullets(self, prompt, context, number_of_bullets=5):

        #   useful 'out of the box' summarize capability with ability to parameterize the number_of_bullets
        #   note: most models are 'approximately' accurate when specifying a number of bullets

        inference_dict = {"number_of_bullets": number_of_bullets}
        output = self.prompt_from_catalog(prompt, context=context,prompt_name="summarize_with_bullets",
                                          inference_dict=inference_dict)

        return output

    def yes_or_no(self, prompt, context):

        #   useful classification prompt, assumes prompt is a question that expects a "yes" or "no" answer
        response = self.prompt_from_catalog(prompt, context=context,prompt_name="yes_no")

        return response

    def completion(self, prompt, temperature=0.7, target_len=200):

        self.llm_model.temperature = temperature
        self.llm_model.ai_max_output_len = target_len

        response = self.prompt_from_catalog(prompt, prompt_name="completion")

        return response

    def multiple_choice(self, prompt, context, choice_list):

        prompt += "\nWhich of the following choices best answers the question - "
        for i, choice in enumerate(choice_list):
            prompt += "(" + chr(65+i) + ") " + choice + ", "

        if prompt.endswith(", "):
            prompt = prompt[:-2] + "?"

        response = self.prompt_from_catalog(prompt, context=context, prompt_name="multiple_choice")

        return response

    def xsummary(self, context, number_of_words=20):

        #   provides an 'extreme summary', e.g., 'xsum' with ability to parameterize the number of words
        #   --most models are reasonably accurate when asking for specific number of words

        prompt=""
        inference_dict = {"number_of_words": number_of_words}
        response = self.prompt_from_catalog(prompt, context=context, prompt_name="xsummary",inference_dict=inference_dict)

        return response

    def title_generator_from_source (self, prompt, context=None, title_only=True):

        response = self.prompt_from_catalog(prompt, context=context,prompt_name="report_title")

        if title_only:
            return response["llm_response"]

        return response

    # core basic prompt inference method
    def prompt_main (self, prompt, prompt_name=None, context=None, call_back_attempts=1, calling_app_id="",
                     prompt_id=0,batch_id=0, trx_dict=None, selected_model= None, register_trx=False,
                     inference_dict=None, max_output=None, temperature=None):

        usage = {}

        if not prompt_name:

            #   pull from .add_prompt_engineering state
            if self.llm_model.add_prompt_engineering:
                prompt_name = self.llm_model.add_prompt_engineering

            else:
                # defaults
                if context:
                    prompt_name = "default_with_context"
                else:
                    prompt_name = "default_no_context"

        if selected_model:
            self.llm_model = ModelCatalog().load_model(selected_model)
        
        if temperature:
            self.temperature = temperature
            
        self.llm_model.temperature = self.temperature

        # if max_output:  self.llm_model.max_tokens = max_output

        if max_output:
            self.llm_max_output_len = max_output

        self.llm_model.target_requested_output_tokens = self.llm_max_output_len
        self.llm_model.add_context = context
        self.llm_model.add_prompt_engineering = prompt_name

        output_dict = self.llm_model.inference(prompt, inference_dict=inference_dict)

        output = output_dict["llm_response"]

        if isinstance(output,list):
            output = output[0]

        # triage process - if output is ERROR code, then keep trying up to parameter- call_back_attempts
        #   by default - will not attempt to triage, e.g., call_back_attempts = 1
        #   --depending upon the calling function, it can decide the criticality and # of attempts

        if output == "/***ERROR***/":
            # try again
            attempts = 1

            while attempts < call_back_attempts:

                # wait 5 seconds to try back
                time.sleep(5)

                # exact same call to inference
                output_dict = self.llm_model.inference(prompt)

                output = output_dict["llm_response"]
                # if list output, then take the string from the first output
                if isinstance(output, list):
                    output = output[0]

                # keep trying until not ERROR message found
                if output != "/***ERROR***/":
                    break

                attempts += 1

            # if could not triage, then present "pretty" error output message
            if output == "/***ERROR***/":
                if "error_message" in output_dict:
                    output = output_dict["error_message"]
                else:
                    output = "AI Output Not Available"

        # strip <s> & </s> which are used by some models as end of text marker
        output = str(output).replace("<s>","")
        output = str(output).replace("</s>","")

        # output = re.sub("<s>","", output)
        # output = re.sub("</s>","", output)

        if "usage" in output_dict:
            usage = output_dict["usage"]

        output_dict = {"llm_response": output, "prompt": prompt,
                       "evidence": context,
                       "instruction": prompt_name, "model": self.llm_model.model_name,
                       "usage": usage,
                       "time_stamp": Utilities().get_current_time_now("%a %b %d %H:%M:%S %Y"),
                       "calling_app_ID": calling_app_id,
                       "rating": "",
                       "account_name": self.account_name,
                       # "library_name": self.library_name,
                       "prompt_id": prompt_id,
                       "batch_id": batch_id,
                        }

        if register_trx:
            self.register_llm_inference(output_dict,prompt_id,trx_dict)

        return output_dict

    # new method
    def _doc_summarizer_old_works(self, query_results, max_batch_size=100, max_batch_cap=None,key_issue=None):

        # runs core summarization loop thru document

        big_batches = len(query_results) // max_batch_size
        # if there was a 'remainder', then run one additional loop ...
        # ... this also picks up the 'normal' case of query_results < max_batch_size
        if len(query_results) > big_batches * max_batch_size:
            big_batches += 1

        response = []

        if max_batch_cap:
            if big_batches > max_batch_cap:

                logging.warning("warning: Prompt document summarization - you have requested a "
                                "maximum cap of %s batches - so truncating the batches from %s to"
                                "the cap requested - note that content will be missing as a result.",
                                max_batch_cap, big_batches)

                big_batches = max_batch_cap

        for x in range(0,big_batches):

            qr = query_results[x*max_batch_size:min((x+1)*max_batch_size,len(query_results))]

            source = self.add_source_query_results(qr)

            if key_issue:
                response += self.prompt_with_source(key_issue, prompt_name="summarize_with_bullets_w_query",
                                                    first_source_only=False)
            else:
                placeholder_issue = "What are the main points?"
                response += self.prompt_with_source(placeholder_issue,prompt_name="summarize_with_bullets",
                                                    first_source_only=False)

        return response

    # new method
    def _doc_summarizer(self, query_results, max_batch_cap=None,key_issue=None):

        # runs core summarization loop thru document
        response = []

        source = self.add_source_query_results(query_results)

        # print("update - len source materials - ", len(self.source_materials))

        if max_batch_cap:
            if len(self.source_materials) > max_batch_cap:

                logging.warning("warning: Prompt document summarization - you have requested a "
                                "maximum cap of %s batches - so truncating the batches from %s to"
                                "the cap requested - note that content will be missing as a result.",
                                max_batch_cap, len(self.source_materials))

                self.source_materials = self.source_materials[0:max_batch_cap]

        if key_issue:
            response += self.prompt_with_source(key_issue, prompt_name="summarize_with_bullets_w_query",
                                                first_source_only=False)
        else:
            placeholder_issue = "What are the main points?"
            response += self.prompt_with_source(placeholder_issue,prompt_name="summarize_with_bullets",
                                                first_source_only=False)

        return response

    # new method - designed to receive one document - and directly summarize
    def summarize_document(self, input_fp,input_fn, query=None, text_only=True, max_batch_cap=10,
                           key_issue=None):

        output = Parser().parse_one(input_fp,input_fn)

        # run in memory filtering to sub-select from document only items matching query
        if query:
            output = Utilities().fast_search_dicts(query, output, remove_stop_words=True)

        response = self._doc_summarizer(output, key_issue=key_issue, max_batch_cap=max_batch_cap)

        if text_only:
            # return only text
            output_text = ""

            for i, entries in enumerate(response):
                # print("update: summaries - ", i, entries)
                output_text += entries["llm_response"] + "\n"

            return output_text

        else:
            return response

    # new method - document summarization from library
    def summarize_document_from_library(self, library, doc_id=None, filename=None, query=None,
                                        text_only=True,max_batch_cap=10):

        # need to handle error
        if not doc_id and not filename:
            placeholder = "no file received"
            return -1

        if doc_id:
            key = "doc_ID"
            value = doc_id
        else:
            key = "file_source"
            value = filename

        if not query:
            if not isinstance(value,list):
                value = [value]

            query_results = Query(library).filter_by_key_value_range(key, value)

        else:
            if isinstance(value,list):
                if len(value) > 0:
                    value = value[0]
            filter_dict = {key:value}
            query_results = Query(library).text_query_with_custom_filter(query,filter_dict,result_count=20)

        response = self._doc_summarizer(query_results, max_batch_cap=max_batch_cap)

        if text_only:
            # return only text
            output_text = ""

            for i, entries in enumerate(response):
                # print("update: summaries - ", i, entries)
                output_text += entries["llm_response"] + "\n"

            return output_text

        else:
            return response

    # useful "call backs"
    def summarize_multiple_responses(self, list_of_response_dict=None, response_id_list=None):

        batch = None

        if list_of_response_dict:
            batch = list_of_response_dict
        elif response_id_list:
            batch = []
            for response_id in response_id_list:
                batch += PromptState(self).lookup_by_prompt_id

        if not batch:
            batch = self.interaction_history

        # batch of response dictionaries -> need to aggregate the llm_responses- and run prompt
        aggregated_response_dict = {}

        return aggregated_response_dict

    def select_among_multiple_responses(self, list_of_response_dict=None, response_id_list=None):

        batch = None

        if list_of_response_dict:
            batch = list_of_response_dict
        elif response_id_list:
            batch = []
            for response_id in response_id_list:
                batch += PromptState(self).lookup_by_prompt_id

        if not batch:
            batch = self.interaction_history

        # batch of response dictionaries -> need to aggregate the llm_responses- and run prompt
        aggregated_response_dict = {}

        return aggregated_response_dict

    # post processing

    def evidence_check_numbers(self, response):

        # expect that response is a list of response dictionaries
        if isinstance(response, dict):
            response = [response]

        response_out = []

        for i, response_dict in enumerate(response):
            qc = QualityCheck(self).fact_checker_numbers(response_dict)

            # print("FACT CHECK - ", qc)

            response_dict.update({"fact_check": qc})
            response_out.append(response_dict)

        return response_out

    def evidence_check_sources(self, response):

        # expect that response is a list of response dictionaries
        if isinstance(response, dict):
            response = [response]

        response_out = []
        for i, response_dict in enumerate(response):
            qc = QualityCheck(self).source_reviewer(response_dict)

            # print("SOURCE REVIEW - ", qc)

            response_dict.update({"source_review": qc})
            response_out.append(response_dict)

        return response_out

    def evidence_comparison_stats(self, response):

        # expect that response is a list of response dictionaries
        if isinstance(response, dict):
            response = [response]

        response_out = []
        for i, response_dict in enumerate(response):
            qc = QualityCheck(self).token_comparison(response_dict)

            # print("COMPARISON STATS - ", qc)

            response_dict.update({"comparison_stats": qc})
            response_out.append(response_dict)

        return response_out

    def classify_not_found_response(self, response_list,parse_response=True,evidence_match=True,ask_the_model=True):

        output_response_all = []

        if isinstance(response_list,dict):
            response_list = [response_list]

        for i, response_dict in enumerate(response_list):
            output_response_all.append(self._classify_not_found_one_response(response_dict,
                                                                             parse_response=parse_response,
                                                                             evidence_match=evidence_match,
                                                                             ask_the_model=ask_the_model))

        return output_response_all

    def _classify_not_found_one_response(self, response_dict, parse_response=True, evidence_match=True, ask_the_model=True):

        output_response = {}
        nf = []

        if parse_response:
            nf1 = QualityCheck(self).classify_not_found_parse_llm_response(response_dict)
            output_response.update({"parse_llm_response": nf1})
            if nf1 not in nf:
                nf.append(nf1)

        if evidence_match:
            nf2 = QualityCheck(self).classify_not_found_evidence_match(response_dict)
            output_response.update({"evidence_match": nf2})
            if nf2 not in nf:
                nf.append(nf2)

        if ask_the_model:
            nf3 = QualityCheck(self).classify_not_found_ask_the_model(response_dict)
            output_response.update({"ask_the_model": nf3})
            if nf3 not in nf:
                nf.append(nf3)

        if len(nf) == 0:
            logging.warning("error: Prompt().classify_not_response() expects at least one of the tests to be marked"
                            "as True - none of the tests were executed - please try again with one test as 'True'")

            return output_response

        # simple case - all of the tests are conforming
        if len(nf) == 1:
            output_response.update({"not_found_classification": nf[0]})
        else:
            output_response.update({"not_found_classification": "undetermined"})

        return output_response

    # user ratings

    def send_to_human_for_review(self, output_path=None, output_fn=None):

        output = HumanInTheLoop(prompt=self).export_current_interaction_to_csv(output_path=output_path,report_name=output_fn)
        return output

    def apply_user_ratings(self, ratings_dict):

        output = HumanInTheLoop(prompt=self).add_or_update_human_rating(self.prompt_id,ratings_dict)
        return output

    def apply_user_corrections(self, updates_dict):

        output = HumanInTheLoop(prompt=self).update_llm_response_record(self.prompt_id,updates_dict,keep_change_log=True)
        return output


class Sources:

    #       Sources can accept any Python iterable consisting of dictionary entries
    #       Each dictionary entry must support minimally the keys in self.source_input_keys
    #       By default, this is a minimum of 3 keys - "text", "file_source", "page_num"

    def __init__(self, prompt):

        self.prompt= prompt
        # self.tokenizer = prompt.tokenizer
        self.tokenizer = Utilities().get_default_tokenizer()

        self.source_input_keys = ["text", "file_source", "page_num"]
        self.source_output_keys = []

        self.source_keys = ["batch_id", "text", "metadata", "biblio", "batch_stats", "batch_stats.tokens",
                            "batch_stats.chars", "batch_stats.samples"]

        self.source_metadata = ["batch_source_num", "evidence_start_char", "evidence_stop_char",
                                "source_name", "page_num"]

    def token_counter(self, text_sample):
        toks = self.tokenizer.encode(text_sample).ids
        return len(toks)

    def tokenize (self, text_sample):
        toks = self.tokenizer.encode(text_sample).ids
        return toks

    def package_source(self, retrieval_material, aggregate_source=True, add_to_prompt=True,
                       backup_source_filename="user_provided_unknown_source"):

        # generalized source packager
        #   --assumes minimal metadata - doc_name, page_num and text chunk
        #   --add to existing 'state' source & create new batch on top if overflow

        # tracking variables
        tokens_per_batch = []
        samples_per_batch = []
        sample_counter = 0
        doc_sources = {}
        doc_sources_per_batch = {}
        biblio_per_batch = []
        batches = []
        meta = []

        samples = []

        for i, q in enumerate(retrieval_material):

            # simple deduplication check to remove identical entries - more 'cleaning' options can be offered over time
            if q not in samples:
                samples.append(q)

        # default
        current_batch = ""
        token_counter = 0
        batch_metadata = []
        batch_id = 0
        char_counter = 0

        if aggregate_source:
            # start current batch with the last entry in source materials and aggregate from this point
            if len(self.prompt.source_materials) > 0:

                # pull up the last 'in-progress' entry in current source materials state
                current_batch = self.prompt.source_materials[-1]["text"]
                token_counter = self.token_counter(current_batch)
                char_counter = len(current_batch)
                batch_metadata = self.prompt.source_materials[-1]["metadata"]
                batch_stats = self.prompt.source_materials[-1]["batch_stats"]
                batch_id = len(self.prompt.source_materials) - 1

                # 'pop' the last entry 'in-progress' off the list
                self.prompt.source_materials = self.prompt.source_materials[:-1]

        samples_chunked = []

        # print("update: input samples len - ", len(samples))

        for x in range(0,len(samples)):

            t = self.token_counter(samples[x]["text"])

            if t > self.prompt.context_window_size:
                chunks = self.chunk_large_sample(samples[x])
                samples_chunked += chunks
            else:
                samples_chunked.append(samples[x])

        samples = samples_chunked

        # print("update: chunked samples len - ", len(samples))

        for x in range(0, len(samples)):

            t = self.token_counter(samples[x]["text"])

            if "file_source" in samples[x]:
                source_fn = samples[x]["file_source"]
            else:
                source_fn = backup_source_filename

            if "page_num" in samples[x]:
                page_num = samples[x]["page_num"]
            else:
                # if can not retrieve from metadata, then set as default - page 1
                page_num = 1

            # keep aggregating text batch up to the size of the target context_window for selected model
            if (t + token_counter) < self.prompt.context_window_size:

                # appends separator at end of sample text before adding the next chunk of text
                current_batch += samples[x]["text"] + self.prompt.batch_separator
                batch_char_len = len(current_batch)

                new_source = {"batch_source_id": len(batch_metadata),
                              "evidence_start_char": char_counter,
                              # remove adding char_counter to evidence_stop_char
                              "evidence_stop_char": batch_char_len,
                              "source_name": source_fn,
                              "page_num": page_num,
                              }

                batch_metadata.append(new_source)

                char_counter = batch_char_len
                token_counter += t

                # new trackers
                sample_counter += 1
                if source_fn not in doc_sources:
                    doc_sources.update({source_fn: [page_num]})
                else:
                    if page_num not in doc_sources[source_fn]:
                        doc_sources[source_fn].append(page_num)

                if source_fn not in doc_sources_per_batch:
                    doc_sources_per_batch.update({source_fn: [page_num]})
                else:
                    if page_num not in doc_sources_per_batch[source_fn]:
                        doc_sources_per_batch[source_fn].append(page_num)

            else:
                # capture number of tokens in batch
                tokens_per_batch.append(token_counter)
                samples_per_batch.append(sample_counter)
                sample_counter = 1

                biblio_per_batch.append(doc_sources_per_batch)

                doc_sources_per_batch = {}

                if "file_source" in samples[x]:
                    doc_filename = samples[x]["file_source"]
                else:
                    doc_filename = backup_source_filename

                if "page_num" in samples[x]:
                    page_num = samples[x]["page_num"]
                else:
                    page_num = 1

                doc_sources_per_batch.update({doc_filename: [page_num]})

                batches.append(current_batch)
                meta.append(batch_metadata)

                if add_to_prompt:
                    new_batch_dict = {"batch_id": batches, "text": current_batch, "metadata": batch_metadata,
                                      "biblio": doc_sources_per_batch, "batch_stats":
                                          {"tokens": token_counter,
                                           "chars": len(current_batch),
                                           "samples": len(batch_metadata)}}

                    self.prompt.source_materials.append(new_batch_dict)

                # reset current_batch -> current snippet
                current_batch = samples[x]["text"]
                token_counter = t
                new_source = {"batch_source_num": 0,
                              "evidence_start_char": 0,
                              "evidence_stop_char": len(samples[x]["text"]),
                              "source_name": source_fn,
                              "page_num": page_num,
                              }

                batch_metadata = [new_source]
                char_counter = len(samples[x]["text"])

        if len(current_batch) > 0:
            batches.append(current_batch)
            meta.append(batch_metadata)

            if add_to_prompt:
                new_batch_dict = {"batch_id": batches, "text": current_batch, "metadata": batch_metadata,
                                  "biblio": doc_sources_per_batch, "batch_stats": {"tokens": token_counter,
                                                                                   "chars": len(current_batch),
                                                                                    "samples": len(batch_metadata)}}

                self.prompt.source_materials.append(new_batch_dict)

            # add new stats for last batch
            tokens_per_batch.append(token_counter)
            samples_per_batch.append(sample_counter)
            biblio_per_batch.append(doc_sources_per_batch)

        new_sources = {"text_batch": batches, "metadata_batch": meta, "batches_count": len(batches)}

        return new_sources

    def chunk_large_sample(self, sample):

        #   if there is a very large single sample, which is bigger than the context window,
        #   then, break up into smaller chunks

        chunks = []
        max_size = self.prompt.context_window_size
        sample_len = self.token_counter(sample["text"])

        chunk_count = sample_len // max_size
        if max_size * chunk_count < sample_len:
            chunk_count += 1

        stopper = 0
        base_dict = {}
        for key, values in sample.items():
            base_dict.update({key:values})

        sample_tokens = self.tokenize(sample["text"])

        for x in range(0,chunk_count):
            starter = stopper
            stopper = min((x+1)*max_size,sample_len)
            new_chunk_tokens = sample_tokens[starter:stopper]
            new_dict = base_dict
            new_dict.update({"text":self.tokenizer.decode(new_chunk_tokens)})
            chunks.append(new_dict)

        # print("update: created sample chunks - ", chunk_count, max_size, sample_len, len(chunks))

        return chunks


class QualityCheck:

    def __init__(self, prompt=None):

        self.llm_response = None
        self.evidence = None
        self.evidence_metadata= None
        self.add_markup = False

        self.prompt = prompt

        # add instruction
        self.instruction = None

        self.comparison_stats = {}
        self.fact_check = {}
        self.ner_fact_check = {}
        self.source_review = {}

    def review (self, response_dict, add_markup=False, review_numbers=True, comparison_stats=True,
                source_review=True, instruction=None):

        self.llm_response = response_dict["llm_response"]
        self.evidence= response_dict["evidence"]
        self.evidence_metadata = response_dict["evidence_metadata"]
        self.add_markup = add_markup

        # add instruction
        self.instruction = instruction

        # review - main entry point into Quality Check - runs several methods for output

        if comparison_stats:
            self.comparison_stats = self.token_comparison (response_dict)

        if review_numbers:
            self.fact_check = self.fact_checker_numbers(response_dict)

        if source_review:
            self.source_review = self.source_reviewer(response_dict)

        return self

    def fact_checker_numbers (self, response_dict):

        ai_gen_output = response_dict["llm_response"]
        evidence = response_dict["evidence"]
        evidence_metadata = response_dict["evidence_metadata"]
        add_markup= False

        # looks for numbers only right now
        llm_response_markup = ""
        fact_check = []

        ai_numbers = []
        ai_numbers_token_tracker = []
        ai_numbers_char_tracker = []

        confirmations = []
        unconfirmations = []

        tokens = ai_gen_output.split(" ")
        percent_on = -1
        char_counter = 0
        for i, tok in enumerate(tokens):

            tok_len = len(tok)

            # minimal cleaning of tokens

            # remove bullet point
            if len(tok) > 0:
                if ord(tok[-1]) == 8226:
                    tok = tok[:-1]

            if len(tok) > 1:
                if tok.startswith("\n"):
                    tok = tok[1:]

            if tok.endswith("\n"):
                tok = tok[:-1]

            if tok.endswith(",") or tok.endswith(".") or tok.endswith("-") or tok.endswith(";") or \
                    tok.endswith(")") or tok.endswith("]"):
                tok = tok[:-1]

            if tok.startswith("$") or tok.startswith("(") or tok.startswith("["):
                tok = tok[1:]

            if tok.endswith("%"):
                tok = tok[:-1]
                percent_on = 1

            tok = re.sub("[,-]","",tok)
            # look for integer numbers - will not find floats
            if Utilities().isfloat(tok):

                if percent_on == 1:
                    tok_fl = float(tok) / 100
                    # turn off
                    percent_on = -1
                else:
                    tok_fl = float(tok)
                ai_numbers.append(tok_fl)
                ai_numbers_token_tracker.append(i)
                ai_numbers_char_tracker.append((char_counter,char_counter+tok_len))

            char_counter += tok_len + 1

        # iterate thru all of the numbers generated - and look for match in evidence
        found_confirming_match = []
        tokens = evidence.split(" ")
        evidence_char_counter = 0
        percent_on = -1
        current_str_token = ""

        for x in range(0, len(ai_numbers)):
            match_tmp = -1
            match_token = -1

            percent_on = -1
            for i, tok in enumerate(tokens):

                tok_len = len(tok)

                if tok.endswith("\n"):
                    tok = tok[:-1]

                current_str_token = tok

                if tok.endswith(",") or tok.endswith(".") or tok.endswith("-") or tok.endswith(";") or \
                        tok.endswith(")") or tok.endswith("]"):
                    tok = tok[:-1]

                if tok.startswith("$") or tok.startswith("(") or tok.startswith("["):
                    tok = tok[1:]

                if tok.endswith("%"):
                    tok = tok[:-1]
                    percent_on = 1

                tok = re.sub("[,-]","",tok)

                if Utilities().isfloat(tok):
                    tok = float(tok)
                    if percent_on == 1:
                        tok = tok / 100
                        # turn off
                        percent_on = -1

                    if tok == ai_numbers[x]:
                        match_token = i

                        if i > 10:
                            start = i-10
                        else:
                            start = 0

                        if i+10 < len(tokens):
                            stop = i+10
                        else:
                            stop = len(tokens)

                        context_window = " ... "
                        for j in range(start,stop):
                            context_window += tokens[j] + " "
                        context_window = re.sub("[\n\r]","",context_window)
                        context_window += " ... "

                        # insert page_num - future update
                        # default - set to the last batch
                        minibatch = len(evidence_metadata)-1
                        for m in range(0,len(evidence_metadata)):

                            starter = evidence_metadata[m]["evidence_start_char"]
                            stopper = evidence_metadata[m]["evidence_stop_char"]
                            if starter <= char_counter <= stopper:
                                minibatch = m
                                break

                        # set default as "NA" - will update once confirmed found in evidence_metadata below
                        page_num = "NA"
                        source_fn = "NA"

                        if len(evidence_metadata[minibatch]) > 1:
                            if "page_num" in evidence_metadata[minibatch]:
                                page_num = evidence_metadata[minibatch]["page_num"]

                            if "source_name" in evidence_metadata[minibatch]:
                                source_fn = evidence_metadata[minibatch]["source_name"]

                        new_fact_check_entry = {"fact": current_str_token,
                                                "status": "Confirmed",
                                                "text": context_window,
                                                "page_num": page_num,
                                                "source": source_fn}
                        fact_check.append(new_fact_check_entry)

                        confirmations.append(current_str_token)

                        match_tmp = 1
                        break

                evidence_char_counter += tok_len + 1

            if match_tmp == -1:
                new_fact_check_entry = {"fact": current_str_token,
                                        "status": "Not Confirmed",
                                        "text": "",
                                        "page_num": "",
                                        "source": ""}

                fact_check.append(new_fact_check_entry)
                unconfirmations.append(current_str_token)

        # provide markup highlighting confirmations and non-confirmations
        confirm_updates = []
        if add_markup:
            for i,f in enumerate(fact_check):

                char_slice = ai_numbers_char_tracker[i]

                # if not confirmed status, then markup as "unconfirm"
                markup_entry = [i, ai_numbers_char_tracker[i], "unconfirm"]

                # test to update mark_up entry to "confirm"
                if len(f) > 1:
                    if "status" in f:
                        if f["status"] == "Confirmed":
                            markup_entry = [i, ai_numbers_char_tracker[i], "confirm"]

                confirm_updates.append(markup_entry)

            confirm_updates = sorted(confirm_updates, key=lambda x:x[0], reverse=True)

            ai_output_markup = ai_gen_output

            for c in confirm_updates:

                output_tmp = ai_output_markup

                if c[2] == "confirm":
                    ai_output_markup = output_tmp[0:c[1][0]] + " <b> "
                    ai_output_markup += output_tmp[c[1][0]:c[1][1]] + " </b> "
                    ai_output_markup += output_tmp[c[1][1]:]
                else:
                    ai_output_markup = output_tmp[0:c[1][0]] + " <font color=red> "
                    ai_output_markup += output_tmp[c[1][0]:c[1][1]] + " </font> "
                    ai_output_markup += output_tmp[c[1][1]:]

            # fact_check.update({"confirmations": confirmations})
            # fact_check.update({"unconfirmations": unconfirmations})
            # fact_check.update({"ai_web_markup": ai_output_markup})

        # note: ai_web_markup not passed

        return fact_check

    def source_reviewer (self, response_dict):

        ai_tmp_output = response_dict["llm_response"]
        evidence_batch = response_dict["evidence"]
        evidence_metadata = response_dict["evidence_metadata"]
        add_markup = False

        # insert test starts here
        # text_snippet_dict = self._evidence_token_matcher(ai_tmp_output, evidence_batch)
        # end - insert test here

        min_th = 0.25
        conclusive_th = 0.75
        min_match_count = 3

        # remove numbers from source review match ???
        c = CorpTokenizer(remove_stop_words=True, one_letter_removal=True, remove_punctuation=True,
                          remove_numbers=False, lower_case=False)

        c2 = CorpTokenizer(remove_stop_words=False, one_letter_removal=False, remove_punctuation=True,
                           remove_numbers=False, lower_case=False)

        # ai_tmp_output = re.sub("[()\"\u201d\u201c]"," ", ai_tmp_output)
        ai_tokens = c.tokenize(ai_tmp_output)
        ai_token_len = len(ai_tokens)

        if ai_token_len == 0:
            # rare case - no ai output, so no need to do any further work
            empty_results = []
            return empty_results

        matching_evidence_score = []
        for x in range(0, len(evidence_metadata)):
            match = 0
            ev_match_tokens = []

            ev_starter = evidence_metadata[x]["evidence_start_char"]
            ev_stopper = evidence_metadata[x]["evidence_stop_char"]

            local_text = evidence_batch[ev_starter:ev_stopper]
            # local_text = re.sub("[()\"\u201d\u201c]", "", local_text)
            evidence_tokens_tmp = c2.tokenize(local_text)
            # evidence_tokens_tmp = local_text.split(" ")

            for tok in ai_tokens:
                for i, etoks in enumerate(evidence_tokens_tmp):
                    if tok.lower() == etoks.lower():
                        match += 1
                        ev_match_tokens.append(i)
                        break

            match_score = match / ai_token_len

            # min threshold to count as source -> % of total or absolute # of matching tokens
            if match_score > min_th or len(ev_match_tokens) > min_match_count:
                matching_evidence_score.append([match_score, x, ev_match_tokens, evidence_tokens_tmp, evidence_metadata[x]["page_num"], evidence_metadata[x]["source_name"]])

        mes = sorted(matching_evidence_score, key=lambda x: x[0], reverse=True)

        sources_output = []
        text_output = []

        if len(mes) > 3:
            top_sources = 3
        else:
            top_sources = len(mes)

        for m in range(0, top_sources):

            page_num = mes[m][4]
            source_name = mes[m][5]
            
            # text_snippet = "Page {}- ... ".format(str(page_num))
            text_snippet = ""

            median_token = int(statistics.median(mes[m][2]))
            if median_token >= 10:
                starter = median_token - 10
            else:
                starter = 0

            if median_token + 10 < len(mes[m][3]):
                stopper = median_token + 10
            else:
                stopper = len(mes[m][3])

            for y in range(starter, stopper):
                text_snippet += str(mes[m][3][y]) + " "

            # text_snippet += " ... "

            text_snippet = re.sub("[\n\r]", " ... ", text_snippet)

            if text_snippet not in text_output:
                text_output.append(text_snippet)

                # new_output = {"text": text_snippet, "match_score": mes[m][0],"source": evidence_metadata[mes[m][1]]}
                new_output = {"text": text_snippet, "match_score": mes[m][0], "source": source_name,
                              "page_num": page_num}

                sources_output.append(new_output)

            if mes[m][0] > conclusive_th:
                # found conclusive source -> no need to look for others
                break

        return sources_output

    # enhanced token comparison
    #   --applies different rules by instruction, e.g., yes-no exclude
    #   --if number in output, looks to handle 'word numbers' + float value comparison
    #   --if multiple points in output, will run comparison separately against each "key point"

    def token_comparison (self, response_dict):

        ai_output_text = response_dict["llm_response"]
        evidence_batch = response_dict["evidence"]
        evidence_metadata = response_dict["evidence_metadata"]

        yes_no = False
        key_point_output_list = []

        if self.instruction == "yes_no":
            yes_no = True

        key_point_list = [ai_output_text]

        c = CorpTokenizer(remove_stop_words=True, remove_numbers=False,one_letter_removal=True, remove_punctuation=False)
        evidence_tokens = c.tokenize(evidence_batch)

        # iterate thru each key point and analyze comparison match
        confirmed_match_agg = []
        unmatched_agg = []
        ai_tokens_agg = []

        evidence_with_numbers = ""
        evidence_numbers_list = []

        for i, kp in enumerate(key_point_list):

            ai_tokens = c.tokenize(kp)
            ai_tokens_agg += ai_tokens

            # skip any empty kp
            if len(ai_tokens) > 0:

                confirmed_match = []
                unmatched = []

                for tok in ai_tokens:
                    match = -1

                    # change starts here - july 19
                    # sharpen matching rules for dollar amounts
                    if tok.endswith("."):
                        tok = tok[:-1]

                    # only remove "." or "," if at the end
                    tok = re.sub("[,();$\"\n\r\t\u2022\u201c\u201d]","",tok)

                    float_check_on = Utilities().isfloat(tok)

                    run_compare = True

                    if float_check_on:
                        if not evidence_with_numbers:

                            evidence_with_numbers, evidence_numbers_list, \
                            token_index_location = Utilities().replace_word_numbers(evidence_batch)

                        for ev_num in evidence_numbers_list:
                            try:
                                if float(ev_num) == float(tok):
                                    confirmed_match.append(tok)
                                    match = 1
                                    run_compare = False
                            except:
                                pass

                    if run_compare:
                        for etoks in evidence_tokens:

                            # change here - mirrrors check in the evidence
                            if etoks.endswith("."):
                                etoks = etoks[:-1]

                            etoks = re.sub("[(),;$\n\r\t\"\u2022\u201c\u201d]","",etoks)

                            # removed lemmatizer and other approximate string matches - look for exact match
                            if tok == etoks:
                                confirmed_match.append(tok)
                                match = 1
                                break

                            # add token compare check if number -> look for numeric equality (even if strings different)
                            if float_check_on:
                                if Utilities().isfloat(etoks):
                                    if float(tok) == float(etoks):
                                        confirmed_match.append(tok)
                                        match = 1
                                        break

                        if match == -1:
                            # no duplicates
                            if tok not in unmatched:
                                unmatched.append(tok)

                # create new entry for kp
                match = len(confirmed_match) / len(ai_tokens)
                new_entry = {"key_point": kp, "entry": len(key_point_output_list), "verified_match": match}
                key_point_output_list.append(new_entry)
                unmatched_agg += unmatched
                confirmed_match_agg += confirmed_match

        # match_percent = 0.0
        match_percent = "{0:.1f}%".format(0.0)
        match_fr = 0.0

        if len(ai_tokens_agg) > 0:

            match_fr = len(confirmed_match_agg) / len(ai_tokens_agg)
            if match_fr > 1.0:
                match_fr = 1.0
            match_percent = "{0:.1f}%".format((match_fr * 100))

        # how to handle, if at all?
        if yes_no and match_fr == 0:
            no_action_for_now = 0

        comparison_stats = {"percent_display": match_percent,
                            "confirmed_words": confirmed_match_agg,
                            "unconfirmed_words": unmatched_agg,
                            "verified_token_match_ratio": match_fr,
                            "key_point_list": key_point_output_list}

        return comparison_stats

    def classify_not_found_parse_llm_response(self, response_dict):

        #   Simple, but reasonably accurate way to classify as "not found" - especially with "not found" instructions
        #   --(1) most models will follow the "not found" instruction and this will be the start of the response
        #   --(2) if a model gets confused and does not provide any substantive response, then this will get flagged too

        # minimal cleaning of response output
        llm_response = response_dict["llm_response"]
        llm_response_cleaned = re.sub("[;!?•(),.\n\r\t\u2022]", "", llm_response).strip().lower()

        # first test:  if no content in 'cleaned' response
        if not llm_response_cleaned:
            return True

        # second test: if response starts with 'not found'
        if llm_response_cleaned.lower().startswith("not found"):
            return True

        return False

    # alternative to calling the model to classify - may be more reliable
    def classify_not_found_evidence_match (self, response_dict, verified_token_match_threshold=0.25):

        #   Objective of this method is to classify a LLM response as "not found"
        #   --this is a key requirement of 'evidence-based' retrieval augmented generation
        #   Note on output:     "True" - indicates that classification of 'Not Found'
        #                       "False" - indicates not 'Not Found' - in other words, use as a valid response

        if "comparison_stats" not in response_dict:
            comparison_stats = self.token_comparison(response_dict)
        else:
            comparison_stats = response_dict["comparison_stats"]

        verified_token_match = comparison_stats["verified_token_match_ratio"]

        # simple threshold passed as parameter - assumes 0.25 as baseline
        #   --e.g., if there is less than 1 in 4 tokens verified in evidence, SKIP
        #   --we could make this higher filter, but occasionally might exclude a valid answer in different format

        llm_response = response_dict["llm_response"]
        llm_response_cleaned = re.sub("[;!?•(),.\n\r\t\u2022]", "", llm_response).strip().lower()

        # carve-out "yes" | "no" answers - special case - will not having 'matching tokens' in evidence
        if llm_response_cleaned in ["yes", "yes.", "no","no."]:
            return False

        if verified_token_match < verified_token_match_threshold:
            return True

        return False

    def classify_not_found_ask_the_model(self, response_dict, selected_model_name=None, model_api_key=None):

        if not selected_model_name:
            selected_model_name = self.prompt.llm_name
            model_api_key = self.prompt.llm_model_api_key

        new_prompt = Prompt().load_model(selected_model_name,api_key=model_api_key)
        new_response = new_prompt.prompt_from_catalog(prompt="", context=response_dict["llm_response"],
                                                      prompt_name="not_found_classifier")

        # print("new response - ", new_response)

        llm_response = new_response["llm_response"]
        llm_response_cleaned = re.sub("[;!?•(),.\n\r\t\u2022]", "", llm_response).strip().lower()

        if llm_response_cleaned.startswith("yes"):
            return True

        if llm_response_cleaned.startswith("no"):
            return False

        #   if the test is inconclusive, then it returns False

        return False


class HumanInTheLoop:

    # add user ratings to queries and analysis + package for 'human-in-the-loop' review

    def __init__(self, prompt, prompt_id_list=None):

        self.prompt= prompt
        self.user_rating_keys = ["human_rating", "human_feedback", "human_assessed_accuracy", "change_log"]

    def export_interaction_to_csv(self, prompt_id_list=None, output_path=None, report_name=None):

        # this method will take a list of one or more prompt_ids and dump to csv for user to review and edit

        output = PromptState(self.prompt).generate_interaction_report(prompt_id_list,
                                                                      output_path=output_path,
                                                                      report_name=report_name)

        return output

    def export_current_interaction_to_csv(self, output_path=None, report_name=None):

        # this method will take the current interaction state and dump to csv ffor user to review and edit

        output = PromptState(self.prompt).generate_interaction_report_current_state(output_path=output_path,
                                                                                    report_name=report_name)

        return output

    # TODO-WIP
    def import_updated_csv(self, fp, fn, prompt_id):

        # allows corrections to be uploaded by csv spreadsheet and corrections made in the history
        return 0

    def add_or_update_human_rating (self, prompt_id, rating_dict):

        rating = -1
        accuracy = ""
        feedback = ""

        f = {"prompt_id": prompt_id}

        if "human_rating" in rating_dict:
            rating = int(rating_dict["human_rating"])

        if "human_feedback" in rating_dict:
            feedback = rating_dict["human_feedback"]

        if "human_assessed_accuracy" in rating_dict:
            accuracy = rating_dict["human_assessed_accuracy"]

        update_dict = {"human_rating": rating, "human_feedback": feedback, "human_assessed_accuracy": accuracy}

        PromptState(self).update_records(prompt_id, f, update_dict)

        return 0

    # new method to allow user to edit/update an llm_response record
    def update_llm_response_record(self,prompt_id, update_dict,keep_change_log=True):

        # as default option, preserve the current values in a change_log list
        #   --over time, we can evaluate whether to capture more metadata about the change, roll-back, etc.

        if keep_change_log:
            # get original record - will save in "change_log" list below changing
            current_record = list(PromptState(self).lookup_by_prompt_id(prompt_id=prompt_id))
            # current_record = list(coll.find(f))

            if len(current_record) == 1:
                current_dict = {}
                for keys in update_dict:
                    if keys in current_record[0]:
                        # this is what will be saved in the list of 'change log' events within the record
                        current_dict.update({keys:current_record[0][keys],
                                             "time_stamp":Utilities().get_current_time_now()})

                if "change_log" in current_record[0]:
                    change_log = current_record[0]["change_log"]
                else:
                    change_log = []
                change_log.append(current_dict)
                update_dict.update({"change_log": change_log})

        # save and update records
        confirmation = PromptState(self).update_records(prompt_id,f,update_dict)

        return confirmation



