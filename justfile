CWD := `pwd`
HOST_USER_UID := `id -u`
HOST_USER_GID := `id -g`

default: (build-win "debug")

_docker_push tag:
    docker push {{tag}}

_docker_build dockerfile tag force="0":
    #!/usr/bin/env sh
    docker images | grep {{tag}} >/dev/null
    if [ $? -eq 0 ] && [ "{{force}}" = "0" ]; then
        echo "Docker image {{dockerfile}} is already built"
    else
        echo "Building Docker image: {{dockerfile}} → {{tag}}"
        docker build \
            --progress plain \
            . \
            -f {{dockerfile}} \
            -t {{tag}}
    fi

_docker_run *args:
    @echo "Running docker image: {{args}}"
    docker run \
        --rm \
        --user \
        {{HOST_USER_UID}}:{{HOST_USER_GID}} \
        --network host \
        -v {{CWD}}:/app/ \
        {{args}}


image-linux force="1":         (_docker_build "tools/docker/game-linux/Dockerfile" "rrdash/tr1x-linux" force)
image-win force="1":           (_docker_build "tools/docker/game-win/Dockerfile" "rrdash/tr1x" force)
image-win-config force="1":    (_docker_build "tools/docker/config/Dockerfile" "rrdash/tr1x-config" force)
image-win-installer force="1": (_docker_build "tools/docker/installer/Dockerfile" "rrdash/tr1x-installer" force)

push-image-linux:              (_docker_push "rrdash/tr1x-linux")
push-image-win:                (_docker_push "rrdash/tr1x")

build-linux target='debug':    (image-linux "0")         (_docker_run "-e" "TARGET="+target "rrdash/tr1x-linux")
build-win target='debug':      (image-win "0")           (_docker_run "-e" "TARGET="+target "rrdash/tr1x")
build-win-config:              (image-win-config "0")    (_docker_run "rrdash/tr1x-config")
build-win-installer:           (image-win-installer "0") (_docker_run "rrdash/tr1x-installer")

package-linux: (build-linux "release")
    #!/bin/sh
    zip_name=$(tools/generate_version)-Linux.zip
    7z a "${zip_name}" ./data/ship/* ./build/linux/TR1X
    echo "Created ${zip_name}"
package-win zip_name="": (build-win "release") (build-win-config)
    #!/bin/sh
    zip_name="{{zip_name}}"
    if [[ -z zip_name ]]; then
        zip_name=$(tools/generate_version)-Windows.zip
    fi
    7z a "${zip_name}" ./data/ship/* ./build/win/TR1X.exe ./tools/config/out/TR1X_ConfigTool.exe
    echo "Created ${zip_name}"
package-win-installer: (package-win "tools/installer/Installer/Resources/release.zip") (build-win-installer)
    #!/bin/sh
    git checkout "tools/installer/Installer/Resources/release.zip"
    exe_name=$(tools/generate_version)-Installer.exe
    cp tools/installer/out/TR1X_Installer.exe "${exe_name}"
    echo "Created ${exe_ame}"

output-current-changelog:
    tools/output_current_changelog

clean:
    -find build/ -type f -delete
    -find tools/ -type f \( -ipath '*/out/*' -or -ipath '*/bin/*' -or -ipath '*/obj/*' \) -delete
    -find . -mindepth 1 -empty -type d -delete

lint-imports:
    tools/sort_imports

lint-format:
    #!/usr/bin/env bash
    shopt -s globstar
    clang-format -i **/*.{c,h}

lint: (lint-imports) (lint-format)
