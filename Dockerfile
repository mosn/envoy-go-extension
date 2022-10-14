FROM envoyproxy/envoy:v1.23-latest

RUN mkdir /usr/local/envoy-go-extension
COPY src/golang/libgolang.so /usr/local/envoy-go-extension/
COPY envoy /usr/local/bin/
COPY envoy-golang.yaml /etc/envoy/

EXPOSE 10000

ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["envoy", "-c", "/etc/envoy/envoy-golang.yaml"]
