import os
import re
import yaml
import psycopg2
from typing import List

from hotmethod_common import FuncStat
from llm_advisor import get_ai_suggestion
from error import AnalysisError, ERR_DB

_RULES_PATH = os.path.join(os.path.dirname(__file__), "rules.yaml")


def _load_rules() -> list:
    with open(_RULES_PATH, "r") as f:
        data = yaml.safe_load(f)
    return data.get("rules", [])


def _match_rule(func_name: str, rules: list) -> str:
    for rule in rules:
        if re.search(rule["pattern"], func_name):
            return rule["suggestion"]
    return ""


def _pg_conn():
    dsn = os.environ.get("PG_DSN", "")
    if not dsn:
        raise AnalysisError(ERR_DB, "PG_DSN environment variable is not set")
    try:
        return psycopg2.connect(dsn)
    except Exception as e:
        raise AnalysisError(ERR_DB, f"cannot connect to PostgreSQL: {e}")


def run_analysis(tid: str, top_funcs: List[FuncStat], enable_llm: bool = True):
    """
    Match each top function against rules.yaml, optionally call the LLM,
    and write the results to the analysis_suggestion table.
    """
    rules = _load_rules()
    conn = _pg_conn()
    try:
        with conn:
            with conn.cursor() as cur:
                for stat in top_funcs:
                    suggestion = _match_rule(stat.name, rules)
                    ai_suggestion = ""

                    if enable_llm and suggestion:
                        try:
                            ai_suggestion = get_ai_suggestion(stat.name, suggestion)
                        except AnalysisError:
                            ai_suggestion = ""

                    status = 1 if (suggestion or ai_suggestion) else 0
                    cur.execute(
                        """
                        INSERT INTO analysis_suggestion
                            (tid, func, suggestion, ai_suggestion, status, created_at)
                        VALUES (%s, %s, %s, %s, %s, NOW())
                        """,
                        (tid, stat.name, suggestion, ai_suggestion, status),
                    )
    except psycopg2.Error as e:
        raise AnalysisError(ERR_DB, f"DB write failed: {e}")
    finally:
        conn.close()
