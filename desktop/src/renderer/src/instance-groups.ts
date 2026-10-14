import type { ApplicationInstance, WorkspaceState } from '@shared/workspace'

export interface InstanceCategory {
  name: string
  id: string
  instances: ApplicationInstance[]
}

export function groupInstancesByCategory(
  state: Pick<WorkspaceState, 'applications' | 'instances'>
): InstanceCategory[] {
  const categories = new Map(
    state.applications.map((application) => [
      application.id,
      application.category.trim() || 'Other'
    ])
  )
  const groups = new Map<string, InstanceCategory>()
  for (const instance of state.instances) {
    const name = categories.get(instance.applicationId) ?? 'Other'
    let group = groups.get(name)
    if (!group) {
      group = { name, id: 'instance-category-' + encodeURIComponent(name), instances: [] }
      groups.set(name, group)
    }
    group.instances.push(instance)
  }
  return [...groups.values()].sort((left, right) => left.name.localeCompare(right.name, 'en'))
}
