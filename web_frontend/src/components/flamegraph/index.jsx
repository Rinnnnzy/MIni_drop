import React from 'react'
import { Empty } from 'antd'

export default function Flamegraph({ url }) {
  if (!url) return <Empty description="火焰图尚未生成" style={{ padding: 60 }} />

  return (
    <iframe
      src={url}
      title="flamegraph"
      style={{ width: '100%', height: 600, border: 'none', borderRadius: 8, background: '#fff' }}
    />
  )
}
