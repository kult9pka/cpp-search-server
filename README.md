# cpp-search-server
## Additional languages: [Русский](Russian/README.md)

Developed a console application - search server. 

### The application works in two steps: 
1.	Adding documents in the system.
2.	Issuance of documents on request ranked by relevance.

### Main features:
*	Stop-words supporting.
*	Minus-words supporting.
* 	Multithread supporting.
* 	Queueing of requests supporting.
*	Multipage output supporting.
*	Finding and deleting duplicates.

### Brief overview of functionality:

* **concurrent_map.h** - class providing thread-safe operation with the map container.
* **document.h** - realisation of the document structure.
* **log_duration.h** - the profiler.
* **paginator.h** - class responsible for multi-paging output of the results of searching.
* **process_queries.h** - realisation of multithreading of the query processing.
* **read_input_functions.h** - realisation of data reading from stream.
* **remove_duplicates.h** - realisation of finding and removing duplicates in database of server.
* **request_queue.h** - request queueing realisation.
* **search_server.h** - realisation of the search server.
* **string_processing.h** - realisation of string processing.
* **test_example_functions.h** - contains tests that cover the basic functionality of the search server. 

*Tests and operation examples reflected in the main.cpp*