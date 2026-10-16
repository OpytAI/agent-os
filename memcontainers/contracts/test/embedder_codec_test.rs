use embedder_rust::{
    AttachmentDefinition, EffectResponse, OperationRequest, OperationResult,
    MC_HOST_EVENT_HOST_CALL, MC_HOST_OK, MC_HOST_OP_EXEC_SHELL,
};

#[test]
fn embedder_frames_are_canonical_and_round_trip() {
    let attachment = AttachmentDefinition {
        kind: 1,
        payload: Vec::new(),
    };
    assert_eq!(
        attachment.encode(),
        [20, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0]
    );
    assert_eq!(
        AttachmentDefinition::decode(&attachment.encode()).unwrap(),
        attachment
    );

    let request = OperationRequest {
        kind: MC_HOST_OP_EXEC_SHELL,
        payload: Vec::new(),
    };
    assert_eq!(request.encode(), [21, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0]);
    assert_eq!(OperationRequest::decode(&request.encode()).unwrap(), request);

    let result = OperationResult {
        kind: MC_HOST_OP_EXEC_SHELL,
        status: MC_HOST_OK as i32,
        payload: Vec::new(),
    };
    assert_eq!(
        result.encode(),
        [22, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    );
    assert_eq!(OperationResult::decode(&result.encode()).unwrap(), result);

    let response = EffectResponse {
        kind: MC_HOST_EVENT_HOST_CALL,
        event_id: 42,
        payload: Vec::new(),
    };
    assert_eq!(
        response.encode(),
        [24, 0, 1, 6, 0, 0, 0, 42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    );
    assert_eq!(EffectResponse::decode(&response.encode()).unwrap(), response);
}

#[test]
fn embedder_frames_fail_closed() {
    let valid = OperationRequest {
        kind: MC_HOST_OP_EXEC_SHELL,
        payload: b"x".to_vec(),
    }
    .encode();

    assert!(OperationRequest::decode(&valid[..valid.len() - 1]).is_err());
    let mut wrong_version = valid.clone();
    wrong_version[2] = 2;
    assert!(OperationRequest::decode(&wrong_version).is_err());
    let mut trailing = valid;
    trailing.push(0);
    assert!(OperationRequest::decode(&trailing).is_err());
}

