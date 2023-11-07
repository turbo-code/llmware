import os
import pytest 
import time

from llmware.library import Library
from llmware.embeddings import EmbeddingHandler
from llmware.exceptions import UnsupportedEmbeddingDatabaseException
from llmware.models import ModelCatalog
from llmware.retrieval import Query
from llmware.setup import Setup
from llmware.configs import LLMWareConfig
from llmware.resources import CloudBucketManager

def test_unsupported_embedding_db():
    embedding_db = "milvusXYZ"  # Bad Embedding DB Name
    with pytest.raises(UnsupportedEmbeddingDatabaseException) as excinfo:
        embedding_handler = EmbeddingHandler(library=None)
        embedding_summary = embedding_handler.create_new_embedding(embedding_db=embedding_db, model=None)
    assert str(excinfo.value) == f"'{embedding_db}' is not a supported vector embedding database" 

def test_milvus_embedding_and_query():
    sample_files_path = Setup().load_sample_files()
    library = Library().create_new_library("test_embedding_milvus")
    library.add_files(os.path.join(sample_files_path,"SmallLibrary"))
    results = generic_embedding_and_query(library, "milvus")
    assert len(results) > 0
    library.delete_library(confirm_delete=True)

def test_faiss_embedding_and_query():
    sample_files_path = Setup().load_sample_files()
    library = Library().create_new_library("test_embedding_faiss")
    library.add_files(os.path.join(sample_files_path,"SmallLibrary"))
    results = generic_embedding_and_query(library, "faiss")
    assert len(results) > 0
    library.delete_library(confirm_delete=True)

# def test_pinecone_embedding_and_query():
#     with pytest.raises(ImportError) as excinfo:
#         library = None
#         results = generic_embedding_and_query(library, "pinecone")
#     assert 'pip install pinecone-client' in str(excinfo.value)

# def test_pinecone_embedding_and_query():
#     sample_files_path = Setup().load_sample_files()
#     library = Library().create_new_library("test_embedding")
#     library.add_files(os.path.join(sample_files_path,"SmallLibrary"))

#     os.environ["PINECONE_API_KEY"] = "abeab29e-7a48-426b-b17a-c74567720876"
#     os.environ["PINECONE_ENVIRONMENT"] = "gcp-starter"

#     results = generic_embedding_and_query(library, "pinecone")
#     assert len(results) > 0
#     library.delete_library(confirm_delete=True)

def generic_embedding_and_query(library, embedding_db):
    # Run the embeddings (only of first 3 docs ) 
    model=ModelCatalog().load_model("mini-lm-sbert")
    embedding_handler = EmbeddingHandler(library=library)
    embedding_summary = embedding_handler.create_new_embedding(embedding_db=embedding_db, model=model,doc_ids=[1, 2, 3, 4, 5])

    # Do a query
    query = "pact"
    query_results = Query(library).semantic_query(query, result_count=5)

    # Delete the embedding
    embedding_handler.delete_index(embedding_db=embedding_db, model=model)
    return query_results