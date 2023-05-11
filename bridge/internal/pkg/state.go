package pkg

type Stage interface {
	Process(data []complex128, metadata Metadata)
}
