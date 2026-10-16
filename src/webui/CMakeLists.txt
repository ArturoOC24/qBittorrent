add_library(qbt_webui STATIC
    # headers
    api/apicontroller.h
    api/apierror.h
    api/apistatus.h
    api/appcontroller.h
    api/authcontroller.h
    api/clientdatacontroller.h
    api/isessionmanager.h
    api/logcontroller.h
    api/synccontroller.h
    api/torrentcreatorcontroller.h
    api/torrentscontroller.h
    api/transfercontroller.h
    api/serialize/serialize_torrent.h
    clientdatastorage.h
    webapplication.h
    websession.h
    webui.h

    # sources
    api/apicontroller.cpp
    api/apierror.cpp
    api/appcontroller.cpp
    api/authcontroller.cpp
    api/clientdatacontroller.cpp
    api/logcontroller.cpp
    api/synccontroller.cpp
    api/torrentcreatorcontroller.cpp
    api/torrentscontroller.cpp
    api/transfercontroller.cpp
    api/serialize/serialize_torrent.cpp
    clientdatastorage.cpp
    webapplication.cpp
    websession.cpp
    webui.cpp
)

if (RSS)
    target_sources(qbt_webui PRIVATE
        api/rsscontroller.h
        api/rsscontroller.cpp
    )
endif()

if (SEARCH)
    target_sources(qbt_webui PRIVATE
        api/searchcontroller.h
        api/searchcontroller.cpp
        searchjobmanager.h
        searchjobmanager.cpp
    )
endif()

target_sources(qbt_webui INTERFACE www/webui.qrc)

target_link_libraries(qbt_webui PRIVATE qbt_base)
