# Copies files into a folder, making it first: cmake -DFILES=<a>|<b> -DTO=<folder> -P CopyIntoFolder.cmake.
# The CTests use it to put a fixture into a project (cmake -E copy needs the folder to exist).
string(REPLACE "|" ";" _files "${FILES}")
file(MAKE_DIRECTORY "${TO}")
file(COPY ${_files} DESTINATION "${TO}")
