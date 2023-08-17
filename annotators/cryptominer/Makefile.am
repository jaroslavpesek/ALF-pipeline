ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS=miner_filter
miner_filter_SOURCES=main.cpp fields.c blacklist.cpp
miner_filter_LDADD=-lunirec -ltrap 
miner_filter_CXXFLAGS=-std=c++2a -pthread -g -Wall -Wextra
include aminclude.am

