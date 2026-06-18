import os

from openai import OpenAI

from error import AnalysisError, ERR_LLM


def get_ai_suggestion(func_name: str, rule_suggestion: str) -> str:
    """
    Call the LLM (Gemini via OpenAI-compatible proxy) for an optimization suggestion.
    Returns an empty string if LLM_API_KEY or LLM_BASE_URL is not configured.
    """
    api_key  = os.environ.get("LLM_API_KEY", "").strip()
    base_url = os.environ.get("LLM_BASE_URL", "").strip()
    model    = os.environ.get("LLM_MODEL", "gemini-1.5-flash")

    if not api_key or not base_url:
        return ""

    client = OpenAI(api_key=api_key, base_url=base_url)

    prompt = (
        f"You are a Linux performance engineering expert. "
        f"During CPU profiling a function named '{func_name}' was identified as a hotspot. "
        f"Preliminary rule-based analysis says: {rule_suggestion}\n\n"
        f"Give a concise, actionable optimization recommendation in 2-3 sentences. "
        f"Focus on concrete code-level changes, not general advice."
    )

    try:
        resp = client.chat.completions.create(
            model=model,
            messages=[{"role": "user", "content": prompt}],
            max_tokens=256,
            temperature=0.3,
        )
        return resp.choices[0].message.content.strip()
    except Exception as e:
        raise AnalysisError(ERR_LLM, f"LLM request failed: {e}")
