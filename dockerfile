FROM gcc:14 AS build

RUN apt-get update && apt-get install -y --no-install-recommends cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" && cmake --build build --target worker --parallel

FROM debian:bookworm-slim

RUN useradd --create-home --uid 10001 worker && mkdir -p /app/result && chown worker:worker /app/result

WORKDIR /app
COPY --from=build /src/build/worker /app/worker

USER worker
VOLUME ["/app/result"]

ENTRYPOINT ["/app/worker"]