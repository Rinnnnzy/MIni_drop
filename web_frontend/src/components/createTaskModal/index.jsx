import React, { useState } from 'react'
import { Modal, Form, Input, InputNumber, Select, message } from 'antd'
import { createTask } from '../../api'

export default function CreateTaskModal({ open, onClose, onCreated }) {
  const [form] = Form.useForm()
  const [loading, setLoading] = useState(false)

  const onOk = async () => {
    let values
    try {
      values = await form.validateFields()
    } catch {
      return // 表单校验失败，antd 自动显示错误
    }
    setLoading(true)
    try {
      await createTask(values)
      message.success('任务创建成功，等待 Agent 采集…')
      form.resetFields()
      onCreated()
    } catch (e) {
      message.error('创建失败：' + (e?.response?.data?.msg || e?.message || '未知错误'))
    } finally {
      setLoading(false)
    }
  }

  const onCancel = () => {
    form.resetFields()
    onClose()
  }

  return (
    <Modal
      title="新建采样任务"
      open={open}
      onCancel={onCancel}
      onOk={onOk}
      confirmLoading={loading}
      okText="创建"
      cancelText="取消"
      width={480}
    >
      <Form
        form={form}
        layout="vertical"
        initialValues={{ hz: 99, duration: 30, callgraph: 'fp', type: 0, profiler_type: 0 }}
        style={{ marginTop: 16 }}
      >
        <Form.Item label="任务名称" name="name">
          <Input placeholder="例：nginx 性能分析" />
        </Form.Item>
        <Form.Item label="目标 Agent IP" name="target_ip" rules={[{ required: true, message: '必填' }]}>
          <Input placeholder="例：192.168.1.100" />
        </Form.Item>
        <Form.Item label="目标进程 PID" name="pid" rules={[{ required: true, message: '必填' }]}>
          <InputNumber style={{ width: '100%' }} placeholder="例：1234" min={1} />
        </Form.Item>
        <Form.Item label="采集时长（秒）" name="duration">
          <InputNumber style={{ width: '100%' }} min={5} max={300} />
        </Form.Item>
        <Form.Item label="采样频率（Hz）" name="hz">
          <InputNumber style={{ width: '100%' }} min={1} max={999} />
        </Form.Item>
        <Form.Item label="调用图类型" name="callgraph">
          <Select options={[
            { value: 'fp',    label: 'Frame Pointer (fp) — 推荐' },
            { value: 'dwarf', label: 'DWARF — 更准确但开销大' },
            { value: 'lbr',   label: 'LBR — 仅 Intel CPU' },
          ]} />
        </Form.Item>
      </Form>
    </Modal>
  )
}
