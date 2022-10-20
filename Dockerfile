FROM envoyproxy/envoy:v1.23-latest

RUN mkdir /usr/local/envoy-go-extension
COPY libgolang.so /usr/local/envoy-go-extension/
COPY envoy /usr/local/bin/
COPY envoy-golang.yaml /etc/envoy/

EXPOSE 10000

ENV GODEBUG=cgocheck=0

ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["envoy", "-c", "/etc/envoy/envoy-golang.yaml"]
