# Builds the handbook's HTML (the page the game's own Help window shows) from index.docbook with xsltproc.
# Run as: cmake -DDOC_DIR=<doc> -DXSLTPROC=<xsltproc> -P BuildHandbook.cmake
#
# index.docbook is the KDE Help Center source and names KDE's DTD, which is not around to be read here: the page is
# processed without it, so the few entities it uses are replaced by hand.
file(READ "${DOC_DIR}/index.docbook" text)
string(REGEX REPLACE "<!DOCTYPE[^]]*\\]>" "" text "${text}")
string(REPLACE "&language;" "en" text "${text}")
string(REPLACE "&times;" "&#215;" text "${text}")
string(REPLACE "&rarr;" "&#8594;" text "${text}")
string(REPLACE "&copy;" "&#169;" text "${text}")
file(MAKE_DIRECTORY "${DOC_DIR}/html")
file(WRITE "${DOC_DIR}/html/index.stripped.xml" "${text}")
execute_process(
  COMMAND "${XSLTPROC}" --nonet -o "${DOC_DIR}/html/index.html" "${DOC_DIR}/handbook-html.xsl" "${DOC_DIR}/html/index.stripped.xml"
  RESULT_VARIABLE result)
file(REMOVE "${DOC_DIR}/html/index.stripped.xml")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "xsltproc failed on the handbook")
endif()
file(COPY "${DOC_DIR}/screenshots" DESTINATION "${DOC_DIR}/html")
