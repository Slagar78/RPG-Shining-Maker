using System.Text;
using System.Text.Json.Nodes;

namespace RpgShinzoMaker.Core.Services;

public static class CJsonFormatter
{
    public static string Format(JsonNode node)
    {
        var sb = new StringBuilder();
        FormatNode(sb, node, 0);
        return sb.ToString();
    }

    private static void FormatNode(StringBuilder sb, JsonNode? node, int depth)
    {
        if (node == null)
        {
            sb.Append("null");
            return;
        }

        if (node is JsonObject obj)
        {
            sb.Append("{");
            if (obj.Count > 0)
            {
                sb.Append("\n");
                int i = 0;
                foreach (var kvp in obj)
                {
                    AppendTabs(sb, depth + 2);
                    sb.Append($"\"{Escape(kvp.Key)}\": ");
                    FormatNode(sb, kvp.Value, depth + 2);
                    if (i < obj.Count - 1) sb.Append(",");
                    sb.Append("\n");
                    i++;
                }
                AppendTabs(sb, depth);
            }
            sb.Append("}");
        }
        else if (node is JsonArray arr)
        {
            sb.Append("[");
            if (arr.Count > 0)
            {
                for (int i = 0; i < arr.Count; i++)
                {
                    if (i > 0) sb.Append(", ");
                    FormatNode(sb, arr[i], depth + 1);
                }
            }
            sb.Append("]");
        }
        else if (node is JsonValue val)
        {
            if (val.TryGetValue<string>(out var str))
                sb.Append($"\"{Escape(str)}\"");
            else
                sb.Append(val.ToJsonString());
        }
    }

    private static void AppendTabs(StringBuilder sb, int count)
    {
        for (int i = 0; i < count; i++) sb.Append('\t');
    }

    private static string Escape(string s)
    {
        if (string.IsNullOrEmpty(s)) return s;
        var sb = new StringBuilder();
        foreach (char c in s)
        {
            switch (c)
            {
                case '\\': sb.Append("\\\\"); break;
                case '\"': sb.Append("\\\""); break;
                case '\n': sb.Append("\\n"); break;
                case '\r': sb.Append("\\r"); break;
                case '\t': sb.Append("\\t"); break;
                default: sb.Append(c); break;
            }
        }
        return sb.ToString();
    }
}