defmodule AgentOS.Git.Public do
  @moduledoc "Public JSON Git host-call adapter. JSON never crosses into the engine."

  alias AgentOS.Contracts.Git
  alias AgentOS.Git.{Json, Transport}
  alias AgentOS.GitEngine

  @status_error Git.status_error()
  @op_status Git.op_status()
  @op_commit Git.op_commit()
  @op_resolve_revision Git.op_resolve_revision()
  @op_ref Git.op_ref()
  @op_file_write Git.op_file_write()
  @op_tag Git.op_tag()
  @op_config Git.op_config()
  @op_remote_metadata Git.op_remote_metadata()
  @op_ignore_query Git.op_ignore_query()
  @op_submodule Git.op_submodule()
  @op_merge Git.op_merge()
  @rendered_ops [Git.op_log(), Git.op_diff(), Git.op_show()]
  @remote_ops [Git.op_clone(), Git.op_fetch(), Git.op_pull(), Git.op_push()]
  @mutation_ops [Git.op_repository_init(), Git.op_add(), Git.op_remove(), Git.op_checkout(), Git.op_reset(), Git.op_branch()]

  @spec mount(binary()) :: {:ok, String.t() | nil} | {:error, term()}
  def mount(body) do
    with {:ok, request} <- decode_request(body) do
      {:ok, request["mount"] || get_in(request, ["args", "mount"])}
    end
  end

  # Dual-host with JS `GUEST_SECRET_ARG_KEYS` / `guestArgsCarrySecrets`.
  @guest_secret_arg_keys MapSet.new([
    "auth",
    "authorization",
    "token",
    "password",
    "credential",
    "credentials",
    "secret"
  ])
  @guest_secret_scan_max_depth 8
  @guest_secret_scan_max_nodes 256

  @spec call(pid(), binary(), keyword()) :: {:ok, binary()} | {:error, term()}
  def call(pid, body, opts) do
    with {:ok, request} <- decode_request(body),
         :ok <- reject_guest_secrets(request),
         {:ok, request} <- resolve_remote_url(pid, request, opts),
         {:ok, request, opts} <- bind_remote(pid, request, opts),
         :ok <- authorize_remote(request, opts),
         {:ok, opcode, payload} <- translate(request, opts),
         {:ok, response} <- GitEngine.request(pid, opcode, payload, opts) do
      {:ok, public_response(opcode, response, request)}
    else
      {:error, reason} -> {:ok, error_response(reason)}
    end
  rescue
    error -> {:ok, error_response(error)}
  end

  defp decode_request(body) do
    with {:ok, %{"op" => op} = request} <- Json.decode(body),
         true <- is_binary(op),
         args when is_map(args) <- Map.get(request, "args", %{}) do
      {:ok, Map.put(request, "args", args)}
    else
      _ -> {:error, :invalid_request}
    end
  end

  defp reject_guest_secrets(%{"args" => args}) do
    case scan_guest_secrets(args, 0, 0) do
      {:ok, _} -> :ok
      _ -> {:error, :guest_secrets_forbidden}
    end
  end

  defp reject_guest_secrets(_), do: :ok

  defp scan_guest_secrets(_value, depth, _nodes) when depth > @guest_secret_scan_max_depth,
    do: :exceeded

  defp scan_guest_secrets(value, depth, nodes) when is_map(value) do
    Enum.reduce_while(value, {:ok, nodes}, fn {key, child}, {:ok, acc} ->
      acc = acc + 1

      cond do
        acc > @guest_secret_scan_max_nodes ->
          {:halt, :exceeded}

        secret_arg_key?(key) ->
          {:halt, :secret}

        true ->
          case scan_guest_secrets(child, depth + 1, acc) do
            {:ok, next} -> {:cont, {:ok, next}}
            other -> {:halt, other}
          end
      end
    end)
  end

  defp scan_guest_secrets(value, depth, nodes) when is_list(value) do
    Enum.reduce_while(value, {:ok, nodes}, fn item, {:ok, acc} ->
      acc = acc + 1

      if acc > @guest_secret_scan_max_nodes do
        {:halt, :exceeded}
      else
        case scan_guest_secrets(item, depth + 1, acc) do
          {:ok, next} -> {:cont, {:ok, next}}
          other -> {:halt, other}
        end
      end
    end)
  end

  defp scan_guest_secrets(_value, _depth, nodes), do: {:ok, nodes}

  defp secret_arg_key?(key) when is_atom(key), do: secret_arg_key?(Atom.to_string(key))

  defp secret_arg_key?(key) when is_binary(key),
    do: MapSet.member?(@guest_secret_arg_keys, String.downcase(key))

  defp secret_arg_key?(_), do: false

  defp bind_remote(_pid, %{"op" => op, "args" => args} = request, opts)
       when op in ["clone", "fetch", "pull", "push"] do
    with {:ok, binding} <- Transport.resolve_remote(args, opts) do
      request = put_in(request, ["args", "url"], binding.url)

      opts =
        opts
        |> Keyword.put(:remote_binding, binding)
        |> Keyword.put(:auth, binding.auth)
        |> Keyword.put(:allowed_origins, binding.origins)

      {:ok, request, opts}
    end
  end

  defp bind_remote(pid, %{"op" => "submodule", "args" => %{"action" => "update"} = args} = request, opts) do
    path = optional_string(Map.get(args, "path"))
    payload = Git.encode_submodule_request(%{action: Git.action_list(), path: path})

    with {:ok, %{status: status, payload: result}} <-
           GitEngine.request(pid, Git.op_submodule(), payload, opts),
         true <- status == Git.status_ok() || {:error, :submodule_list_failed},
         {:ok, %{entries: entries}} <- Git.decode_submodule_result(result),
         {:ok, bindings} <- resolve_submodule_bindings(entries, args, opts) do
      {:ok, request, Keyword.put(opts, :remote_bindings, bindings)}
    else
      {:error, _} = error -> error
      _ -> {:error, :submodule_list_failed}
    end
  end

  defp bind_remote(_pid, request, opts), do: {:ok, request, opts}

  defp resolve_submodule_bindings(entries, args, opts) do
    reference = args["connection"] || args["agentos"]

    Enum.reduce_while(entries, {:ok, []}, fn entry, {:ok, acc} ->
      remote_args = %{"url" => entry.url}

      remote_args =
        if is_binary(reference) and reference != "",
          do: Map.put(remote_args, "connection", reference),
          else: remote_args

      case Transport.resolve_remote(remote_args, opts) do
        {:ok, binding} -> {:cont, {:ok, [binding | acc]}}
        {:error, reason} -> {:halt, {:error, reason}}
      end
    end)
    |> case do
      {:ok, bindings} -> {:ok, Enum.reverse(bindings)}
      error -> error
    end
  end

  defp authorize_remote(%{"op" => op} = request, opts) when op in ["clone", "fetch", "pull", "push"] do
    cond do
      op == "push" and Keyword.get(opts, :read_only, false) == true ->
        {:error, :read_only}

      op == "push" ->
        authorize_push(request, opts)

      true ->
        :ok
    end
  end

  defp authorize_remote(%{"op" => "submodule", "args" => %{"action" => "update"}}, opts) do
    if Keyword.get(opts, :read_only, false) == true, do: {:error, :read_only}, else: :ok
  end

  defp authorize_remote(_request, _opts), do: :ok

  defp authorize_push(%{"args" => args}, opts) do
    policies = Keyword.get(opts, :policies, [])

    if not is_list(policies) do
      {:error, :invalid_policies}
    else
      authorize_push_action(args, policies, opts)
    end
  end

  defp authorize_push_action(args, policies, opts) do
    ref = connection_ref(args)

    case evaluate_push_policy(ref, policies) do
      :block ->
        {:error, :push_blocked}

      :require_approval ->
        callback = Keyword.get(opts, :on_push_approval)
        context = %{url: args["url"], connection_ref: optional_connection_ref(ref)}

        if is_function(callback, 1) and callback.(context) == true do
          :ok
        else
          {:error, :push_approval_required}
        end

      :approve ->
        :ok
    end
  end

  defp connection_ref(args) when is_map(args) do
    case args["connection"] || args["agentos"] do
      ref when is_binary(ref) and ref != "" -> ref
      _ -> "*"
    end
  end

  defp optional_connection_ref("*"), do: nil
  defp optional_connection_ref(ref), do: ref

  @doc false
  def evaluate_push_policy(_ref, policies) when not is_list(policies), do: :block
  def evaluate_push_policy(_ref, []), do: :approve

  def evaluate_push_policy(ref, policies) when is_binary(ref) and is_list(policies) do
    Enum.reduce(policies, :approve, fn rule, worst ->
      pattern = policy_field(rule, :pattern)

      if is_binary(pattern) and match_connection_pattern?(pattern, ref) do
        more_restrictive(worst, normalize_push_action(policy_field(rule, :action)))
      else
        worst
      end
    end)
  end

  defp match_connection_pattern?("*", _ref), do: true
  defp match_connection_pattern?(pattern, ref) when pattern == ref, do: true

  defp match_connection_pattern?(pattern, ref) do
    if String.ends_with?(pattern, ".*") do
      prefix = String.slice(pattern, 0, byte_size(pattern) - 2)
      ref == prefix or String.starts_with?(ref, prefix <> ".")
    else
      false
    end
  end

  defp more_restrictive(a, b) do
    rank = %{approve: 0, require_approval: 1, block: 2}
    if Map.fetch!(rank, b) > Map.fetch!(rank, a), do: b, else: a
  end

  defp normalize_push_action(action) when action in [:approve, :require_approval, :block],
    do: action

  defp normalize_push_action("approve"), do: :approve
  defp normalize_push_action("require_approval"), do: :require_approval
  defp normalize_push_action("block"), do: :block
  defp normalize_push_action(_), do: :block

  defp policy_field(rule, key) when is_map(rule) do
    Map.get(rule, key, Map.get(rule, Atom.to_string(key)))
  end

  defp policy_field({pattern, _action}, :pattern) when is_binary(pattern), do: pattern
  defp policy_field({pattern, action}, :action) when is_binary(pattern), do: action
  defp policy_field(_rule, _key), do: nil

  defp translate(%{"op" => "init"}, _), do: {:ok, Git.op_repository_init(), <<>>}
  defp translate(%{"op" => "status"}, _), do: {:ok, Git.op_status(), <<>>}
  defp translate(%{"op" => "write", "args" => args}, _) do
    path = required_string(args, "path")
    data = args["content"] || args["data"] || ""
    request = %{path: path, other_path: nil, mode: nil, offset_low: nil, offset_high: nil, data: data, handle: nil}
    {:ok, Git.op_file_write(), Git.encode_file_request(request)}
  end
  defp translate(%{"op" => op, "args" => args}, _) when op in ["add", "rm"] do
    paths = paths(args)
    opcode = if op == "add", do: Git.op_add(), else: Git.op_remove()
    {:ok, opcode, porcelain(action: Git.action_update(), paths: paths)}
  end
  defp translate(%{"op" => "commit", "args" => args}, opts) do
    with message when is_binary(message) <- args["message"],
         {:ok, identity} <- identity(args, opts) do
      now = integer(args["when_unix"], System.system_time(:second))
      signature = %{name: identity.name, email: identity.email, unix_seconds: now, timezone_minutes: integer(args["timezone_minutes"], 0)}
      {:ok, Git.op_commit(), porcelain(action: Git.action_create(), message: message, author: signature, committer: signature)}
    else
      _ -> {:error, :commit_identity_required}
    end
  end
  defp translate(%{"op" => "rev-parse", "args" => args}, _), do: {:ok, Git.op_resolve_revision(), porcelain(action: Git.action_get(), revision: string(args["rev"], "HEAD"))}
  defp translate(%{"op" => op, "args" => args}, _) when op in ["checkout", "switch"] do
    target = optional_string(args["name"])
    revision = optional_string(args["rev"] || args["revision"])

    if target == nil and revision == nil do
      raise ArgumentError, "missing name"
    end

    {:ok, Git.op_checkout(),
     porcelain(
       action: Git.action_update(),
       target: target,
       revision: revision,
       flags: integer(args["flags"], 0)
     )}
  end
  defp translate(%{"op" => "reset", "args" => args}, _) do
    action = %{
      "soft" => Git.reset_soft(),
      "mixed" => Git.reset_mixed(),
      "hard" => Git.reset_hard(),
      "merge" => Git.reset_merge()
    }[string(args["mode"], "mixed")]
    if action, do: {:ok, Git.op_reset(), porcelain(action: action, revision: string(args["rev"], "HEAD"))}, else: {:error, :invalid_reset_mode}
  end
  defp translate(%{"op" => "branch", "args" => args}, _) when map_size(args) == 0 do
    {:ok, Git.op_ref(), porcelain(action: Git.action_list())}
  end
  defp translate(%{"op" => "log", "args" => args}, _) do
    {:ok, Git.op_log(), porcelain(action: Git.action_list(), revision: args["rev"], limit: integer(args["max_count"], 32))}
  end
  defp translate(%{"op" => "diff", "args" => args}, _) do
    flags = if args["cached"] == true, do: 1, else: 0
    {:ok, Git.op_diff(), porcelain(action: Git.action_get(), flags: flags, revision: args["rev"], paths: paths(args))}
  end
  defp translate(%{"op" => "show", "args" => args}, _) do
    {:ok, Git.op_show(), porcelain(action: Git.action_get(), revision: string(args["rev"], "HEAD"))}
  end
  defp translate(%{"op" => "config", "args" => %{"action" => "list"}}, _) do
    {:ok, Git.op_config(), porcelain(action: Git.action_list())}
  end
  defp translate(%{"op" => "config", "args" => args}, _) do
    key = required_config_key(args["key"])
    action = case args["action"] do "get" -> Git.action_get(); "set" -> Git.action_update(); "remove" -> Git.action_delete(); _ -> raise ArgumentError, "invalid config action" end
    {:ok, Git.op_config(), porcelain(action: action, target: key, message: args["value"])}
  end
  defp translate(%{"op" => "remote", "args" => %{"action" => "list"}}, _) do
    {:ok, Git.op_remote_metadata(), porcelain(action: Git.action_list())}
  end
  defp translate(%{"op" => "remote", "args" => args}, _) do
    action = case args["action"] do "get" -> Git.action_get(); "add" -> Git.action_create(); "set" -> Git.action_update(); "remove" -> Git.action_delete(); _ -> raise ArgumentError, "invalid remote action" end
    {:ok, Git.op_remote_metadata(), porcelain(action: action, target: required_string(args, "name"), message: args["url"])}
  end
  defp translate(%{"op" => "check-ignore", "args" => args}, _) do
    path = required_string(args, "path")
    {:ok, Git.op_ignore_query(), Git.encode_path_query(%{paths: %{path => ""}})}
  end
  defp translate(%{"op" => "submodule", "args" => args}, _) do
    action =
      case string(args["action"], "list") do
        "list" -> Git.action_list()
        "status" -> Git.action_get()
        "update" -> Git.action_update()
        _ -> raise ArgumentError, "unsupported Git submodule action"
      end

    path = optional_string(args["path"])

    if action != Git.action_update() and path != nil do
      raise ArgumentError, "Git submodule path is only valid for update"
    end

    {:ok, Git.op_submodule(), Git.encode_submodule_request(%{action: action, path: path})}
  end
  defp translate(%{"op" => op, "args" => args}, _) when op in ["branch", "tag"] do
    action = if args["delete"] == true, do: Git.action_delete(), else: Git.action_create()
    opcode = if op == "branch", do: Git.op_branch(), else: Git.op_tag()
    {:ok, opcode, porcelain(action: action, target: required_string(args, "name"), revision: args["rev"])}
  end
  defp translate(%{"op" => "merge", "args" => args}, opts) do
    action = merge_action(args["action"])

    with {:ok, signature} <- merge_signature(action, args, opts) do
      {:ok, Git.op_merge(), Git.encode_merge_request(merge_fields(args, action, signature))}
    end
  end
  defp translate(%{"op" => op, "args" => args}, opts) when op in ["clone", "fetch", "pull", "push"] do
    opcode = %{"clone" => Git.op_clone(), "fetch" => Git.op_fetch(), "pull" => Git.op_pull(), "push" => Git.op_push()}[op]
    url = remote_url(args, opts)
    refspecs = refspecs(args)
    if op == "push" and map_size(refspecs) != 1, do: raise(ArgumentError, "Git push requires exactly one refspec")
    depth = remote_depth(args["depth"], op != "push")
    {:ok, opcode, Git.encode_remote_request(%{action: Git.action_begin(), url: url, remote: args["remote"], refspecs: refspecs, depth: depth, flags: 0})}
  end
  defp translate(_, _), do: {:error, :unsupported_operation}

  defp porcelain(overrides) do
    defaults = %{flags: 0, revision: nil, target: nil, message: nil, paths: %{}, limit: nil, cursor: nil, author: nil, committer: nil}
    defaults |> Map.merge(Map.new(overrides)) |> Git.encode_porcelain_request()
  end

  defp paths(%{"paths" => values}) when is_list(values), do: Map.new(values, &{to_string(&1), ""})
  defp paths(%{"path" => path}) when is_binary(path), do: %{path => ""}
  defp paths(%{"all" => true}), do: %{"." => ""}
  defp paths(_), do: %{}
  defp refspecs(%{"refspecs" => [value]}), do: parse_refspec(value)
  defp refspecs(%{"refspecs" => []}), do: %{}
  defp refspecs(%{"refspecs" => values}) when is_list(values), do: raise(ArgumentError, "Git push requires exactly one refspec")
  defp refspecs(%{"refspec" => value}), do: parse_refspec(value)
  defp refspecs(_), do: %{}
  defp remote_url(%{"url" => url}, _) when is_binary(url) and url != "", do: url
  defp remote_url(args, opts) do
    reference = args["connection"] || args["agentos"]

    opts
    |> Keyword.get(:connections, [])
    |> Enum.find(fn connection -> field(connection, :ref) == reference end)
    |> case do
      nil -> ""
      connection ->
        spec = field(connection, :spec) || %{}
        field(spec, :url) || field(spec, :baseUrl) || ""
    end
  end
  defp field(map, key) when is_map(map), do: Map.get(map, key, Map.get(map, Atom.to_string(key)))
  defp resolve_remote_url(pid, %{"op" => op, "args" => args} = request, opts)
       when op in ["fetch", "pull", "push"] do
    if is_binary(args["url"]) and args["url"] != "" do
      {:ok, request}
    else
      name = string(args["remote"], "origin")
      payload = porcelain(action: Git.action_get(), target: name)

      with {:ok, %{status: status, payload: result_payload}} <-
             GitEngine.request(pid, Git.op_remote_metadata(), payload, opts),
           true <- status == Git.status_ok(),
           {:ok, %{data: url}} when is_binary(url) and url != "" <-
             Git.decode_result(result_payload) do
        {:ok, put_in(request, ["args", "url"], url)}
      else
        _ -> {:error, :remote_url_required}
      end
    end
  end
  defp resolve_remote_url(_pid, request, _opts), do: {:ok, request}
  defp parse_refspec(value) when is_binary(value) and byte_size(value) <= 2048 do
    if String.contains?(value, ["\0", "\r", "\n"]) do
      raise ArgumentError, "invalid Git refspec"
    end

    case String.split(value, ":") do
      ["refs/" <> source_tail = source, "refs/" <> target_tail = target]
      when source_tail != "" and target_tail != "" ->
        %{source => target}

      _ ->
        raise ArgumentError,
              "Git refspec must be one fully-qualified source:destination pair"
    end
  end
  defp parse_refspec(_), do: raise(ArgumentError, "invalid Git refspec")
  defp merge_action(nil), do: Git.action_create()
  defp merge_action("start"), do: Git.action_create()
  defp merge_action("create"), do: Git.action_create()
  defp merge_action("abort"), do: Git.action_abort()
  defp merge_action("continue"), do: Git.action_finish()
  defp merge_action("finish"), do: Git.action_finish()
  defp merge_action(_), do: raise(ArgumentError, "invalid merge action")

  defp merge_signature(action, args, opts) do
    if action == Git.action_abort() do
      {:ok, nil}
    else
      case identity(args, opts) do
        {:ok, %{name: name, email: email}} ->
          {:ok,
           %{
             name: name,
             email: email,
             unix_seconds: integer(args["when_unix"] || args["unix_seconds"], System.system_time(:second)),
             timezone_minutes: integer(args["timezone_minutes"], 0)
           }}

        {:error, :missing_identity} ->
          {:error, :missing_identity}
      end
    end
  end

  defp merge_fields(args, action, signature) do
    %{
      action: action,
      heads: if(action == Git.action_create(), do: merge_heads(args["heads"]), else: []),
      strategy: merge_choice(args["strategy"], &merge_strategy_name/1, Git.merge_strategy_default()),
      fast_forward: merge_choice(args["ff"] || args["fast_forward"], &merge_ff_name/1, Git.merge_ff()),
      favor: merge_choice(args["favor"], &merge_favor_name/1, Git.merge_favor_none()),
      diff_algorithm: merge_choice(args["diff_algorithm"], &merge_diff_name/1, Git.merge_diff_histogram()),
      conflict_style: merge_choice(args["conflict_style"], &merge_style_name/1, Git.merge_style_merge()),
      find_renames: args["find_renames"] != false,
      no_commit: args["no_commit"] == true,
      allow_unrelated_histories: args["allow_unrelated_histories"] == true,
      rename_threshold: merge_threshold(args["rename_threshold"]),
      subtree_path: merge_optional_string(args["subtree"] || args["subtree_path"]),
      message: merge_optional_string(args["message"]),
      author: signature,
      committer: signature
    }
  end

  defp merge_heads(heads) when is_list(heads) and heads != [] do
    Enum.map(heads, fn
      revision when is_binary(revision) and revision != "" -> %{revision: revision}
      %{"revision" => revision} when is_binary(revision) and revision != "" -> %{revision: revision}
      _ -> raise ArgumentError, "invalid merge head"
    end)
  end

  defp merge_heads(_), do: raise(ArgumentError, "merge requires at least one revision")

  defp merge_choice(nil, _fun, default), do: default
  defp merge_choice(value, fun, _default) when is_binary(value), do: fun.(value)
  defp merge_choice(_, _, _), do: raise(ArgumentError, "invalid merge option")

  defp merge_strategy_name("ort"), do: Git.merge_strategy_ort()
  defp merge_strategy_name("recursive"), do: Git.merge_strategy_recursive()
  defp merge_strategy_name("resolve"), do: Git.merge_strategy_resolve()
  defp merge_strategy_name("octopus"), do: Git.merge_strategy_octopus()
  defp merge_strategy_name("ours"), do: Git.merge_strategy_ours()
  defp merge_strategy_name("subtree"), do: Git.merge_strategy_subtree()
  defp merge_strategy_name(_), do: raise(ArgumentError, "invalid merge strategy")

  defp merge_ff_name("ff"), do: Git.merge_ff()
  defp merge_ff_name("no-ff"), do: Git.merge_ff_no_ff()
  defp merge_ff_name("ff-only"), do: Git.merge_ff_only()
  defp merge_ff_name(_), do: raise(ArgumentError, "invalid merge fast-forward")

  defp merge_favor_name("none"), do: Git.merge_favor_none()
  defp merge_favor_name("ours"), do: Git.merge_favor_ours()
  defp merge_favor_name("theirs"), do: Git.merge_favor_theirs()
  defp merge_favor_name(_), do: raise(ArgumentError, "invalid merge favor")

  defp merge_diff_name("histogram"), do: Git.merge_diff_histogram()
  defp merge_diff_name("myers"), do: Git.merge_diff_myers()
  defp merge_diff_name("minimal"), do: Git.merge_diff_minimal()
  defp merge_diff_name("patience"), do: Git.merge_diff_patience()
  defp merge_diff_name(_), do: raise(ArgumentError, "invalid merge diff algorithm")

  defp merge_style_name("merge"), do: Git.merge_style_merge()
  defp merge_style_name("diff3"), do: Git.merge_style_diff3()
  defp merge_style_name("zdiff3"), do: Git.merge_style_zdiff3()
  defp merge_style_name(_), do: raise(ArgumentError, "invalid merge conflict style")

  defp merge_threshold(nil), do: nil
  defp merge_threshold(value) when is_integer(value) and value >= 0 and value <= 100, do: value
  defp merge_threshold(_), do: raise(ArgumentError, "invalid merge rename threshold")

  defp merge_optional_string(nil), do: nil
  defp merge_optional_string(value) when is_binary(value) and value != "", do: value
  defp merge_optional_string(_), do: raise(ArgumentError, "invalid merge string")

  defp merge_body(result) do
    cond do
      result.outcome == Git.merge_outcome_conflicts() ->
        {1, false, merge_conflict_lines(result.conflicts), "merge conflicts\n"}

      result.outcome in [Git.merge_outcome_committed(), Git.merge_outcome_fast_forward()] ->
        {0, true, merge_object_line(result.object_id), ""}

      true ->
        {0, true, "", ""}
    end
  end

  defp merge_conflict_lines(conflicts) do
    Enum.map_join(conflicts, "", fn row ->
      <<row.ours::utf8, row.theirs::utf8>> <> " " <> row.path <> "\n"
    end)
  end

  defp merge_object_line(nil), do: ""
  defp merge_object_line(%{bytes: bytes}), do: Base.encode16(bytes, case: :lower) <> "\n"

  defp identity(args, opts) do
    configured = Keyword.get(opts, :identity, %{})
    name = args["name"] || configured[:name] || configured["name"]
    email = args["email"] || configured[:email] || configured["email"]
    if is_binary(name) and name != "" and is_binary(email) and email != "", do: {:ok, %{name: name, email: email}}, else: {:error, :missing_identity}
  end
  defp required_string(map, key), do: if(is_binary(map[key]) and map[key] != "", do: map[key], else: raise(ArgumentError, "missing #{key}"))
  defp string(value, _default) when is_binary(value), do: value
  defp string(_, default), do: default
  defp integer(value, _) when is_integer(value), do: value
  defp integer(_, default), do: default
  defp remote_depth(nil, _permitted), do: nil
  defp remote_depth(_value, false), do: raise(ArgumentError, "Git push does not accept depth")
  defp remote_depth(value, true) when is_integer(value) and value >= 1 and value <= 0xFFFFFFFF,
    do: value
  defp remote_depth(_value, true), do: raise(ArgumentError, "Git depth must be a positive u32 integer")
  defp optional_string(nil), do: nil
  defp optional_string(value) when is_binary(value) and value != "", do: value
  defp optional_string(_), do: raise(ArgumentError, "invalid Git path")
  defp required_config_key(key) when key in ["user.name", "user.email"], do: key
  defp required_config_key(_), do: raise(ArgumentError, "unsupported Git config key")

  defp public_response(_opcode, %{status: @status_error, payload: payload}, _request) do
    error =
      case Git.decode_engine_error(payload) do
        {:ok, value} ->
          %{
            "domain" => value.domain,
            "code" => value.code,
            "operation" => value.operation,
            "retry" => value.retry,
            "message" => value.message || "git operation failed"
          }

        _ ->
          %{"domain" => 0, "code" => 0, "operation" => 0, "retry" => 0, "message" => "git operation failed"}
      end

    Json.encode(%{
      "ok" => false,
      "code" => 1,
      "stdout" => "",
      "stderr" => error["message"] <> "\n",
      "error" => error
    })
  end
  defp public_response(@op_merge, %{payload: payload}, _request) do
    case Git.decode_merge_result(payload) do
      {:ok, result} ->
        {code, ok, stdout, stderr} = merge_body(result)

        Json.encode(%{
          "ok" => ok,
          "code" => code,
          "stdout" => stdout,
          "stderr" => stderr,
          "result" => %{
            "generation" => result.generation,
            "outcome" => result.outcome,
            "object_id" => object_id_hex(result.object_id),
            "conflicts" =>
              Enum.map(result.conflicts, fn row ->
                %{"path" => row.path, "ours" => row.ours, "theirs" => row.theirs}
              end)
          }
        })

      _ ->
        Json.encode(%{"ok" => false, "code" => 1, "stdout" => "", "stderr" => "git operation failed\n"})
    end
  end
  defp public_response(opcode, %{payload: payload}, request) do
    {stdout, result} = decode_success(opcode, payload)
    stdout = public_stdout(request, stdout, result)
    Json.encode(%{"ok" => true, "code" => 0, "stdout" => stdout, "stderr" => "", "result" => result})
  end
  defp public_stdout(%{"op" => "submodule", "args" => %{"action" => "update"}}, _stdout, %{"submodules" => entries}),
    do: "updated #{length(entries)} submodule(s)\n"
  defp public_stdout(_, stdout, _), do: stdout
  defp decode_success(@op_status, payload) do
    case Git.decode_status_result(payload) do
      {:ok, %{entries: entries}} -> {Enum.map_join(entries, "\n", &"#{<<&1.index>>}#{<<&1.worktree>>} #{&1.path}") <> if(entries == [], do: "", else: "\n"), %{"entries" => entries}}
      _ -> {"", %{}}
    end
  end
  defp decode_success(opcode, payload) when opcode in [@op_commit, @op_resolve_revision] do
    decoded = if opcode == @op_commit, do: Git.decode_commit_result(payload), else: Git.decode_resolve_result(payload)
    case decoded do {:ok, %{object_id: oid}} -> hex = Base.encode16(oid.bytes, case: :lower); {hex <> "\n", %{"oid" => hex}}; _ -> {"", %{}} end
  end
  defp decode_success(@op_ref, payload) do
    case Git.decode_reference_list(payload) do
      {:ok, %{references: references}} ->
        names = Enum.map(references, & &1.name)
        {Enum.join(names, "\n") <> if(names == [], do: "", else: "\n"), %{"references" => names}}
      _ -> {"", %{}}
    end
  end
  defp decode_success(@op_file_write, payload) do
    case Git.decode_file_result(payload) do
      {:ok, file} -> {"", %{"path" => file.path, "mode" => file.mode, "size" => file.size_low + Bitwise.bsl(file.size_high, 32)}}
      _ -> {"", %{}}
    end
  end
  defp decode_success(@op_ignore_query, payload) do
    case Git.decode_ignore_result(payload) do
      {:ok, %{paths: paths}} ->
        names = Map.keys(paths)
        {Enum.join(names, "\n") <> if(names == [], do: "", else: "\n"), %{"paths" => paths}}

      _ ->
        {"", %{}}
    end
  end
  defp decode_success(@op_submodule, payload) do
    case Git.decode_submodule_result(payload) do
      {:ok, %{generation: generation, entries: entries}} ->
        rendered = Enum.map(entries, &submodule_entry/1)

        stdout =
          Enum.map_join(rendered, "", fn entry ->
            oid = entry["gitlink"] || String.duplicate("-", 40)
            prefix = if entry["head"] != nil and entry["head"] == oid, do: " ", else: "-"
            prefix <> oid <> " " <> entry["path"] <> "\n"
          end)

        {stdout, %{"generation" => generation, "submodules" => rendered}}

      _ ->
        {"", %{}}
    end
  end
  defp decode_success(opcode, payload) when opcode in @remote_ops do
    case Git.decode_remote_result(payload) do
      {:ok, result} ->
        updated = Enum.map(result.updated, & &1.name)
        {"", %{"handle" => result.handle, "state" => result.state, "generation" => result.generation, "updated" => updated}}
      _ -> {"", %{}}
    end
  end
  defp decode_success(opcode, payload) when opcode in @mutation_ops do
    case Git.decode_result(payload) do
      {:ok, result} -> {"", %{"generation" => result.generation, "count" => result.count}}
      _ -> {"", %{}}
    end
  end
  defp decode_success(@op_tag, payload) do
    case Git.decode_reference_result(payload) do
      {:ok, result} -> {result.name <> "\n", %{"name" => result.name}}
      _ ->
        case Git.decode_result(payload) do
          {:ok, result} -> {"", %{"generation" => result.generation, "count" => result.count}}
          _ -> {"", %{}}
        end
    end
  end
  defp decode_success(opcode, payload) when opcode in [@op_config, @op_remote_metadata] do
    case Git.decode_result(payload) do
      {:ok, result} -> {result.data || "", %{"generation" => result.generation, "count" => result.count}}
      _ -> {"", %{}}
    end
  end
  defp decode_success(opcode, payload) when opcode in @rendered_ops do
    case Git.decode_result(payload) do
      {:ok, result} -> {result.data || "", %{"generation" => result.generation, "count" => result.count}}
      _ -> {"", %{}}
    end
  end
  defp decode_success(_, _), do: {"", %{}}
  defp submodule_entry(entry) do
    %{
      "name" => entry.name,
      "path" => entry.path,
      "url" => entry.url,
      "gitlink" => object_id_hex(entry.gitlink),
      "head" => object_id_hex(entry.head),
      "state" => entry.state
    }
  end
  defp object_id_hex(nil), do: nil
  defp object_id_hex(%{bytes: bytes}) when is_binary(bytes), do: Base.encode16(bytes, case: :lower)

  defp error_response(:unknown_connection),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: unknown_connection\n"
      })

  defp error_response(:origin_not_allowed),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: origin_not_allowlisted\n"
      })

  defp error_response(:query_auth_unsupported),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: query_auth_unsupported\n"
      })

  defp error_response(:bad_remote_url),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: remote url must be http(s) without embedded credentials\n"
      })

  defp error_response(:invalid_policies),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: invalid push policies\n"
      })

  defp error_response(:guest_secrets_forbidden),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: credential material is forbidden in guest Git requests\n"
      })

  defp error_response(:push_blocked),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: Git push is blocked by policy\n"
      })

  defp error_response(:push_approval_required),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: Git push approval is required\n"
      })

  defp error_response(:read_only),
    do:
      Json.encode(%{
        "ok" => false,
        "code" => 1,
        "stdout" => "",
        "stderr" => "git: read-only Git mount\n"
      })

  defp error_response(reason),
    do: Json.encode(%{"ok" => false, "code" => 2, "stdout" => "", "stderr" => "git: #{inspect(reason)}\n"})
end
