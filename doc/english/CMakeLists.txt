add_custom_command(OUTPUT fuse-cpp-ramfs.ps
                   COMMAND groff -e -T ps -man ${CMAKE_CURRENT_SOURCE_DIR}/fuse-cpp-ramfs.man > fuse-cpp-ramfs.ps
                   DEPENDS fuse-cpp-ramfs.man
                   COMMENT "Creating postscript file")
add_custom_target(postscript ALL
                  DEPENDS fuse-cpp-ramfs.ps)

# Set GROFF_NO_SGR because the following txt rule running ul is incompatible with the
# default.
add_custom_command(OUTPUT fuse-cpp-ramfs.1
                   COMMAND export GROFF_NO_SGR=1 && groff -e -T ascii -man ${CMAKE_CURRENT_SOURCE_DIR}/fuse-cpp-ramfs.man > fuse-cpp-ramfs.1
                   DEPENDS fuse-cpp-ramfs.man
                   COMMENT "Creating man page")
add_custom_target(man ALL
                  DEPENDS fuse-cpp-ramfs.1)

add_custom_command(OUTPUT fuse-cpp-ramfs.txt
                   COMMAND ul -t dumb fuse-cpp-ramfs.1 > fuse-cpp-ramfs.txt
                   DEPENDS fuse-cpp-ramfs.1
                   COMMENT "Creating text file")
add_custom_target(txt ALL
                  DEPENDS fuse-cpp-ramfs.txt)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fuse-cpp-ramfs.ps" DESTINATION "${DOCDIR}")
install(FILES fuse-cpp-ramfs.man DESTINATION "${MANDIR}/man1" RENAME fuse-cpp-ramfs.1)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fuse-cpp-ramfs.txt" DESTINATION "${DOCDIR}")
