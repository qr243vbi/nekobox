TARGET ?=
TARGET_FLAG = $(if $(TARGET),--target=$(TARGET),)

build:
	go run -v ./cmd/build-naive build $(TARGET_FLAG)
	go run -v ./cmd/build-naive package --local $(TARGET_FLAG)
	go run -v ./cmd/build-naive package $(TARGET_FLAG)

apple:
	TARGET="ios/arm64,ios/arm64/simulator,ios/amd64/simulator,tvos/arm64,tvos/arm64/simulator,tvos/amd64/simulator,darwin/arm64,darwin/amd64" make

generate_net_errors:
	go run ./cmd/build-naive generate-net-errors

test: build
	cd test && go test -a -count=1 -v .

fmt:
	@find . -name '*.go' -not -path './naiveproxy/*' -exec gofumpt -l -w {} +
	@find . -name '*.go' -not -path './naiveproxy/*' -exec gofmt -s -w {} +
	@gci write --custom-order -s standard -s "prefix(github.com/sagernet/)" -s "default" $$(find . -name '*.go' -not -path './naiveproxy/*')

fmt_install:
	go install -v mvdan.cc/gofumpt@v0.8.0
	go install -v github.com/daixiang0/gci@latest

lint:
	golangci-lint run

lint_install:
	go install -v github.com/golangci/golangci-lint/v2/cmd/golangci-lint@v2.4.0
