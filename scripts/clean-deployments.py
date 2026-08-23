import sys
import json
import urllib.request
import urllib.error

def clean_repo_deployments(repo, token, keep_latest=1):
    print(f"\n=======================================================")
    print(f"  Очистка деплоев для: {repo}")
    print(f"=======================================================")
    
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "User-Agent": "DeployCleaner",
        "X-GitHub-Api-Version": "2022-11-28"
    }

    # 1. Fetch all deployments (handle pagination up to 100)
    url = f"https://api.github.com/repos/{repo}/deployments?per_page=100"
    req = urllib.request.Request(url, headers=headers)
    
    try:
        with urllib.request.urlopen(req) as resp:
            deps = json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        print(f"Ошибка запроса списка деплоев ({e.code}): {e.read().decode()}")
        return
    except Exception as e:
        print(f"Ошибка соединения: {e}")
        return

    total = len(deps)
    print(f"Найдено деплоев: {total}")
    if total <= keep_latest:
        print(f"Деплоев {total} <= {keep_latest}, удаление не требуется.")
        return

    to_delete = deps[keep_latest:]
    print(f"Будет удалено старых деплоев: {len(to_delete)} (сохраняем последний {keep_latest})\n")

    deleted_count = 0
    for dep in to_delete:
        dep_id = dep["id"]
        created = dep.get("created_at", "")
        
        # Шаг 1: Сделать неактивным
        status_url = f"https://api.github.com/repos/{repo}/deployments/{dep_id}/statuses"
        status_payload = json.dumps({"state": "inactive"}).encode('utf-8')
        status_req = urllib.request.Request(status_url, data=status_payload, headers=headers, method="POST")
        try:
            with urllib.request.urlopen(status_req) as st_resp:
                pass
        except Exception:
            pass

        # Шаг 2: Удалить
        del_url = f"https://api.github.com/repos/{repo}/deployments/{dep_id}"
        del_req = urllib.request.Request(del_url, headers=headers, method="DELETE")
        try:
            with urllib.request.urlopen(del_req) as del_resp:
                if del_resp.status in (204, 200):
                    print(f"  ✓ Удален деплой ID {dep_id} ({created})")
                    deleted_count += 1
        except urllib.error.HTTPError as e:
            print(f"  ✗ Ошибка при удалении {dep_id} ({e.code}): {e.read().decode()}")
        except Exception as e:
            print(f"  ✗ Ошибка при удалении {dep_id}: {e}")

    print(f"\nУспешно удалено: {deleted_count} из {len(to_delete)}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Использование: python scripts/clean-deployments.py <GITHUB_TOKEN> [repo1] [repo2] ...")
        print("\nПример:")
        print("  python scripts/clean-deployments.py ghp_xxxxxxxxxxxx hotredman/numkit-demo hotredman/numkit-doxy")
        sys.exit(1)

    token = sys.argv[1].strip()
    repos = sys.argv[2:] if len(sys.argv) > 2 else ["hotredman/numkit-demo", "hotredman/numkit-doxy"]

    for r in repos:
        clean_repo_deployments(r, token, keep_latest=1)

    print("\nОчистка завершена!")
